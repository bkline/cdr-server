"""
HTTPS interface for handling CDR client-server requests

Pulled out separate from the main CGI script for pre-compilation.
"""

import base64
import datetime
import os
import sys
from lxml import etree
from cdrapi.users import Session
from cdrapi.settings import Tier
from cdrapi.docs import Doc, Doctype, FilterSet, LinkType
from cdrapi.searches import QueryTermDef

from six import itervalues
try:
    basestring
    base64encode = base64.encodestring
    base64decode = base64.decodestring
except:
    base64encode = base64.encodebytes
    base64decode = base64.decodebytes
    basestring = str, bytes
    unicode = str


class CommandSet:
    PARSER = etree.XMLParser(strip_cdata=False)
    COMMANDS = dict(
        CdrAddAction="_add_action",
        CdrAddDoc="_add_doc",
        CdrAddDocType="_add_doctype",
        CdrAddExternalMapping="_add_external_mapping",
        CdrAddFilterSet="_add_filter_set",
        CdrAddGrp="_add_group",
        CdrAddLinkType="_put_link_type",
        CdrAddQueryTermDef="_add_query_term_def",
        CdrAddUsr="_put_user",
        CdrCanDo="_can_do",
        CdrCheckAuth="_check_auth",
        CdrDelAction="_del_action",
        CdrDelDoc="_del_doc",
        CdrDelDocType="_del_doctype",
        CdrDelFilterSet="_del_filter_set",
        CdrDelGrp="_del_group",
        CdrDelLinkType="_del_linktype",
        CdrDelQueryTermDef="_del_query_term_def",
        CdrDelUsr="_del_user",
        CdrDupSession="_dup_session",
        CdrFilter="_filter_doc",
        CdrGetAction="_get_action",
        CdrGetCssFiles="_get_css_files",
        CdrGetDoc="_get_doc",
        CdrGetDocType="_get_doctype",
        CdrGetFilters="_get_filters",
        CdrGetFilterSet="_get_filter_set",
        CdrGetFilterSets="_get_filter_sets",
        CdrGetGrp="_get_group",
        CdrGetLinkType="_get_link_type",
        CdrGetUsr="_get_user",
        CdrListDocTypes="_list_doctypes",
        CdrListGrps="_list_groups",
        CdrListLinkProps="_list_linkprops",
        CdrListLinkTypes="_list_linktypes",
        CdrListQueryTermDefs="_list_query_term_defs",
        CdrListQueryTermRules="_list_query_term_rules",
        CdrListSchemaDocs="_list_schema_docs",
        CdrListUsrs="_list_users",
        CdrLogoff="_logoff",
        CdrListActions="_list_actions",
        CdrModDocType="_mod_doctype",
        CdrModGrp="_mod_group",
        CdrModLinkType="_put_link_type",
        CdrModUsr="_put_user",
        CdrRepAction="_mod_action",
        CdrRepDoc="_rep_doc",
        CdrRepFilterSet="_rep_filter_set",
        CdrValidateDoc="_validate_doc",
    )

    def __init__(self):
        self.tier = Tier()
        self.level = os.environ.get("HTTP_X_LOGGING_LEVEL", "INFO")
        self.logger = self.tier.get_logger("https_api", level=self.level)
        self.start = datetime.datetime.now()
        self.session = None
        self.client = os.environ.get("REMOTE_ADDR")
        self.root = self.__load_commands()

    def get_responses(self):
        responses = []
        for node in self.root.findall("*"):
            if node.tag == "SessionId":
                try:
                    name = node.text.strip()
                    self.session = Session(name)
                except Exception as e:
                    self.logger.warning(str(e))
                    return self.__wrap_responses(self.__wrap_error(e))
            elif node.tag == "CdrCommand":
                try:
                    responses.append(self.__process_command(node))
                except:
                    self.logger.exception("{} failure".format(node.tag))
            else:
                self.logger.warning("unexpected element %r", node.tag)
        return self.__wrap_responses(*responses)


    #------------------------------------------------------------------
    # MAPPED COMMAND METHODS START HERE.
    #------------------------------------------------------------------

    def _add_action(self, node):
        name = flag = comment = None
        for child in node.findall("*"):
            if child.tag == "Name":
                name = child.text.strip()
            elif child.tag == "DoctypeSpecific":
                flag = child.text
            elif child.tag == "Comment":
                comment = child.text
        action = Session.Action(name, flag, comment)
        action.add(self.session)
        return etree.Element("CdrAddActionResp")

    def _add_doc(self, node):
        try:
            return self.__put_doc(node, new=True)
        except:
            self.logger.exception("_add_doc()")
            raise

    def _add_doctype(self, node):
        return self.__put_doctype(node, new=True)

    def _add_external_mapping(self, node):
        usage = cdr_id = value = None
        opts = dict(bogus="N", mappable="Y")
        for child in node.findall("*"):
            if child.tag == "Usage":
                usage = self.get_node_text(child)
            elif child.tag == "CdrId":
                cdr_id = self.get_node_text(child)
            elif child.tag == "Value":
                value = self.get_node_text(child)
            elif child.tag == "Bogus":
                opts["bogus"] = self.get_node_text(child)
            elif child.tag == "Mappable":
                opts["mappable"] = self.get_node_text(child)
        doc = Doc(self.session, id=cdr_id)
        mapping_id = str(doc.add_external_mapping(usage, value, **opts))
        return etree.Element("CdrAddExternalMappingResp", MappingId=mapping_id)

    def _add_filter_set(self, node):
        return self.__put_filter_set(node, new=True)

    def _add_group(self, node):
        opts = dict(name=None, comment=None, users=[], actions={})
        for child in node.findall("*"):
            if child.tag == "GrpName":
                opts["name"] = child.text.strip()
            elif child.tag == "Comment":
                opts["comment"] = child.text
            elif child.tag == "UserName":
                opts["users"].append(child.text)
            elif child.tag == "Auth":
                action = self.get_node_text(child.find("Action"), "").strip()
                doctype = self.get_node_text(child.find("DocType"), "").strip()
                if not action:
                    raise Exception("Missing Action element in auth")
                if action not in opts["actions"]:
                    opts["actions"][action] = []
                opts["actions"][action].append(doctype)
        group = Session.Group(**opts)
        group.add(self.session)
        return etree.Element("CdrAddGrpResp")

    def _add_query_term_def(self, node):
        path = self.get_node_text(node.find("Path"))
        rule = self.get_node_text(node.find("Rule")) or None
        QueryTermDef(self.session, path, rule).add()
        return etree.Element("CdrAddQueryTermDefResp")

    def _can_do(self, node):
        action = doc_type = None
        for child in node.findall("*"):
            if child.tag == "Action":
                action = child.text.strip()
            elif child.tag == "DocType":
                doc_type = child.text.strip()
        if not action:
            raise Exception("No action specified to check")
        response = etree.Element("CdrCanDoResp")
        response.text = "Y" if self.session.can_do(action, doc_type) else "N"
        return response

    def _check_auth(self, node):
        pairs = []
        for wrapper in node.findall("Auth"):
            action = self.get_node_text(wrapper.find("Action"))
            doctype = self.get_node_text(wrapper.find("DocType"))
            pairs.append((action, doctype))
        response = etree.Element("CdrCheckAuthResp")
        permissions = self.session.check_permissions(pairs)
        for action in sorted(permissions):
            added = False
            for doctype in sorted(permissions[action]):
                if doctype and doctype.strip():
                    wrapper = etree.SubElement(response, "Auth")
                    etree.SubElement(wrapper, "Action").text = action
                    etree.SubElement(wrapper, "DocType").text = doctype
                    added = True
            if not added:
                wrapper = etree.SubElement(response, "Auth")
                etree.SubElement(wrapper, "Action").text = action
        return response

    def _del_action(self, node):
        name = self.get_node_text(node.find("Name"), "").strip()
        if not name:
            raise Exception("Missing action name")
        self.session.get_action(name).delete(self.session)
        return etree.Element("CdrDelActionResp")

    def _del_doc(self, node):
        opts = {}
        doc = None
        for child in node.findall("*"):
            if child.tag == "DocId":
                doc = Doc(self.session, id=child.text)
            elif child.tag == "Validate":
                opts["validate"] = child.text == "Y"
            elif child.tag == "Reason":
                opts["reason"] = child.text
        if doc is None:
            raise Exception("Missing document ID")
        doc.delete(**opts)
        response = etree.Element("CdrDelDocResp")
        etree.SubElement(response, "DocId").text = doc.cdr_id
        if doc.errors:
            response.append(doc.errors_node)
        return response

    def _del_doctype(self, node):
        name = node.get("Type")
        if not name:
            raise Exception("Missing doctype name")
        Doctype(self.session, name=name).delete()
        return etree.Element("CdrDelDocTypeResp")

    def _del_filter_set(self, node):
        name = self.get_node_text(node.find("FilterSetName"))
        filter_set = FilterSet(self.session, name=name)
        filter_set.delete()
        return etree.Element("CdrDelFilterSetResp")

    def _del_group(self, node):
        name = self.get_node_text(node.find("GrpName"), "").strip()
        if not name:
            raise Exception("Missing group name")
        self.session.get_group(name).delete(self.session)
        return etree.Element("CdrDelGrpResp")

    def _del_linktype(self, node):
        name = self.get_node_text(node.find("Name"))
        LinkType(self.session, name=name).delete()
        return etree.Element("CdrDelLinkTypeResp")

    def _del_query_term_def(self, node):
        path = self.get_node_text(node.find("Path"))
        rule = self.get_node_text(node.find("Rule")) or None
        QueryTermDef(self.session, path, rule).delete()
        return etree.Element("CdrDelQueryTermDefResp")

    def _del_user(self, node):
        name = self.get_node_text(node.find("UserName"))
        Session.User(self.session, name=name).delete()
        return etree.Element("CdrDelUsrResp")

    def _dup_session(self, node):
        session = self.session.duplicate()
        response = etree.Element("CdrDupSessionResp")
        etree.SubElement(response, "SessionId").text = self.session.name
        etree.SubElement(response, "NewSessionId").text = session.name
        return response

    def _filter_doc(self, node):
        output = node.get("Output") != "N"
        opts = {
            "output": output,
            "ctl": node.get("ctl") == "y",
            "version": node.get("FilterVersion"),
            "date": node.get("FilterCutoff")
        }
        doc = None
        filters = []
        parms = {}
        for child in node.findall("*"):
            if child.tag == "Document":
                href = child.get("href")
                if href:
                    ver = child.get("version")
                    date = child.get("maxDate")
                    doc = Doc(self.session, id=href, version=ver, before=date)
                else:
                    doc = Doc(self.session, xml=self.get_node_text(doc_node))
            elif child.tag == "Filter":
                href = child.get("href")
                name = child.get("Name")
                if href:
                    filters.append(href)
                elif name:
                    filters.append("name:{}".format(name))
                else:
                    opts["filter"] = self.get_node_text(child)
                    raise Exception("filter without ID or name")
            elif child.tag == "FilterSet":
                filters.append("set:{}".format(child.get("Name")))
            elif child.tag == "Parm":
                name = self.get_node_text(child.find("Name"), "")
                value = self.get_node_text(child.find("Value"), "")
                parms[name] = value
        if not doc:
            raise Exception("nothing to filter")
        result = doc.filter(*filters, **opts)
        response = etree.Element("CdrFilterResp")
        if output:
            doc = unicode(result.result_tree)
            etree.SubElement(response, "Document").text = etree.CDATA(doc)
        if result.messages:
            messages = etree.SubElement(response, "Messages")
            for message in result.messages:
                etree.SubElement(messages, "message").text = message
        return response

    def _get_action(self, node):
        name = self.get_node_text(node.find("Name"), "").strip()
        if not name:
            raise Exception("Missing action name")
        action = self.session.get_action(name)
        if not action:
            raise Exception("Action not found: {}".format(name))
        flag = action.doctype_specific
        response = etree.Element("CdrGetActionResp")
        etree.SubElement(response, "Name").text = action.name
        etree.SubElement(response, "DoctypeSpecific").text = flag
        etree.SubElement(response, "Comment").text = action.comment or ""
        return response

    def _get_css_files(self, node):
        files = Doctype.get_css_files(self.session)
        response = etree.Element("CdrGetCssFilesResp")
        for name in sorted(files):
            data = base64encode(files[name]).decode("ascii")
            wrapper = etree.SubElement(response, "File")
            etree.SubElement(wrapper, "Name").text = name
            etree.SubElement(wrapper, "Data").text = data
        return response

    def _get_doc(self, node):
        doc_id = self.get_node_text(node.find("DocId"))
        version = self.get_node_text(node.find("DocVersion"))
        denormalize = self.get_node_text(node.find("DenormalizeLinks"), "Y")
        lock = self.get_node_text(node.find("Lock")) == "Y"
        doc = Doc(self.session, id=doc_id, version=version)
        if lock:
            doc.check_out()
        opts = {
            "get_xml": node.get("includeXml", "Y") == "Y",
            "get_blob": node.get("includeBlob", "Y") == "Y",
            "denormalize": denormalize != "N"
        }
        response = etree.Element("CdrGetDocResp")
        response.append(doc.legacy_doc(**opts))
        return response

    def _get_doctype(self, node):
        name = node.get("Type")
        include_values = node.get("GetEnumValues") == "Y"
        include_dtd = node.get("OmitDtd") != "Y"
        self.logger.info("_get_doctype(): name=%s", name)
        doctype = Doctype(self.session, name=name)
        response = etree.Element("CdrGetDocTypeResp")
        response.set("Type", doctype.name)
        response.set("Format", doctype.format)
        response.set("Versioning", doctype.versioning)
        response.set("Created", str(doctype.created))
        response.set("SchemaMod", str(doctype.schema_mod))
        response.set("Active", doctype.active)
        if include_dtd:
            dtd = etree.CDATA(doctype.dtd)
            etree.SubElement(response, "DocDtd").text = dtd
        etree.SubElement(response, "DocSchema").text = doctype.schema
        if include_values:
            vv_lists = doctype.vv_lists
            for set_name in sorted(vv_lists):
                wrapper = etree.SubElement(response, "EnumSet", Node=set_name)
                for value in vv_lists[set_name]:
                    etree.SubElement(wrapper, "ValidValue").text = value
        linking_elements = doctype.linking_elements
        if linking_elements:
            wrapper = etree.SubElement(response, "LinkingElements")
            for name in sorted(linking_elements):
                etree.SubElement(wrapper, "LinkingElement").text = name
        if doctype.comment is not None:
            etree.SubElement(response, "Comment").text = doctype.comment
        return response

    def _get_filters(self, node):
        response = etree.Element("CdrGetFiltersResp")
        for doc in FilterSet.get_filters(self.session):
            node = etree.SubElement(response, "Filter", DocId=doc.cdr_id)
            node.text = doc.title
        return response

    def _get_filter_set(self, node):
        name = self.get_node_text(node.find("FilterSetName"))
        filter_set = FilterSet(self.session, name=name)
        response = etree.Element("CdrGetFilterSetResp")
        etree.SubElement(response, "FilterSetName").text = filter_set.name
        if filter_set.description is not None:
            child = etree.SubElement(response, "FilterSetDescription")
            child.text = filter_set.description
        if filter_set.notes is not None:
            child = etree.SubElement(response, "FilterSetNotes")
            child.text = filter_set.notes
        for m in filter_set.members:
            if isinstance(m, Doc):
                child = etree.SubElement(response, "Filter", DocId=m.cdr_id)
                child.text = m.title
            else:
                set_id = str(m.id)
                child = etree.SubElement(response, "FilterSet", SetId=set_id)
                child.text = m.name
        return response

    def _get_filter_sets(self, node):
        response = etree.Element("CdrGetFilterSetsResp")
        for id, name in FilterSet.get_filter_sets(self.session):
            etree.SubElement(response, "FilterSet", SetId=str(id)).text = name
        return response

    def _get_group(self, node):
        name = self.get_node_text(node.find("GrpName"), "").strip()
        if not name:
            raise Exception("Missing group name")
        group = self.session.get_group(name)
        if not group:
            raise Exception("Group not found: {}".format(name))
        response = etree.Element("CdrGetGrpResp")
        etree.SubElement(response, "GrpName").text = group.name
        etree.SubElement(response, "GrpId").text = str(group.id)
        for action in sorted(group.actions):
            doctypes = group.actions[action] or [""]
            for doctype in doctypes:
                auth = etree.SubElement(response, "Auth")
                etree.SubElement(auth, "Action").text = action
                if doctype:
                    etree.SubElement(auth, "DocType").text = doctype
        for user in group.users:
            etree.SubElement(response, "UserName").text = user
        if group.comment:
            etree.SubElement(response, "Comment").text = group.comment
        return response

    def _get_link_type(self, node):
        name = self.get_node_text(node.find("Name"), "").strip()
        if not name:
            raise Exception("Missing link type name")
        linktype = LinkType(self.session, name=name)
        response = etree.Element("CdrGetLinkTypeResp")
        etree.SubElement(response, "Name").text = linktype.name
        etree.SubElement(response, "LinkChkType").text = linktype.chk_type
        if linktype.comment is not None:
            comment = linktype.comment
            etree.SubElement(response, "LinkTypeComment").text = comment
        sources = linktype.sources or []
        targets = linktype.targets or []
        properties = linktype.properties or []
        for source in sources:
            wrapper = etree.SubElement(response, "LinkSource")
            etree.SubElement(wrapper, "SrcDocType").text = source.doctype.name
            etree.SubElement(wrapper, "SrcField").text = source.element
            for target in itervalues(linktype.targets):
                etree.SubElement(response, "TargetDocType").text = target.name
        for prop in properties:
            wrapper = etree.SubElement(response, "LinkProperties")
            etree.SubElement(wrapper, "LinkProperty").text = prop.name
            etree.SubElement(wrapper, "PropertyValue").text = prop.value
            etree.SubElement(wrapper, "PropertyComment").text = prop.comment
        return response

    def _get_user(self, node):
        name = self.get_node_text(node.find("UserName"))
        user = Session.User(self.session, name=name)
        response = etree.Element("CdrGetUsrResp")
        etree.SubElement(response, "UserName").text = user.name
        etree.SubElement(response, "AuthenticationMode").text = user.authmode
        if user.fullname is not None:
            etree.SubElement(response, "FullName").text = user.fullname
        if user.office is not None:
            etree.SubElement(response, "Office").text = user.office
        if user.email is not None:
            etree.SubElement(response, "Email").text = user.email
        if user.phone is not None:
            etree.SubElement(response, "Phone").text = user.phone
        if user.comment is not None:
            etree.SubElement(response, "Comment").text = user.comment
        for group in (user.groups or []):
            etree.SubElement(response, "GrpName").text = group
        return response

    def _list_actions(self, node):
        response = etree.Element("CdrListActionsResp")
        for action in self.session.list_actions():
            flag = action.doctype_specific
            action_node = etree.SubElement(response, "Action")
            etree.SubElement(action_node, "Name").text = action.name
            etree.SubElement(action_node, "NeedDoctype").text = flag
        return response

    def _list_doctypes(self, node):
        response = etree.Element("CdrListDocTypesResp")
        for name in Doctype.list_doc_types(self.session):
            etree.SubElement(response, "DocType").text = name
        return response

    def _list_groups(self, node):
        response = etree.Element("CdrListGrpsResp")
        for name in self.session.list_groups():
            etree.SubElement(response, "GrpName").text = name
        return response

    def _list_linkprops(self, node):
        response = etree.Element("CdrListLinkPropsResp")
        for prop_type in LinkType.get_property_types(self.session):
            wrapper = etree.SubElement(response, "LinkProperty")
            etree.SubElement(wrapper, "Name").text = prop_type.name
            if prop_type.comment is not None:
                etree.SubElement(wrapper, "Comment").text = prop_type.comment
        return response

    def _list_linktypes(self, node):
        response = etree.Element("CdrListLinkTypesResp")
        for name in LinkType.get_linktype_names(self.session):
            etree.SubElement(response, "Name").text = name
        return response

    def _list_query_term_defs(self, node):
        response = etree.Element("CdrListQueryTermDefsResp")
        for definition in QueryTermDef.get_definitions(self.session):
            wrapper = etree.SubElement(response, "Definition")
            etree.SubElement(wrapper, "Path").text = definition.path
            if definition.rule is not None:
                etree.SubElement(wrapper, "Rule").text = definition.rule
        return response

    def _list_query_term_rules(self, node):
        response = etree.Element("CdrListQueryTermRulesResp")
        for rule in QueryTermDef.get_rules(self.session):
            etree.SubElement(response, "Rule").text = rule
        return response

    def _list_schema_docs(self, node):
        response = etree.Element("CdrListSchemaDocsResp")
        for title in Doctype.list_schema_docs(self.session):
            etree.SubElement(response, "DocTitle").text = title
        return response

    def _list_users(self, node):
        response = etree.Element("CdrListUsrsResp")
        for name in self.session.list_users():
            etree.SubElement(response, "UserName").text = name
        return response

    def _logoff(self, node):
        self.session.logout()
        return etree.Element("CdrLogoffResp")

    def _mod_action(self, node):
        name = self.get_node_text(node.find("Name"))
        new_name = self.get_node_text(node.find("NewName"))
        flag = self.get_node_text(node.find("DoctypeSpecific"))
        comment = self.get_node_text(node.find("Comment"))
        action = self.session.get_action(name)
        if new_name:
            action.name = new_name
        action.doctype_specific = flag
        action.comment = comment
        action.modify(self.session)
        return etree.Element("CdrRepActionResp")

    def _mod_doctype(self, node):
        return self.__put_doctype(node, new=False)

    def _mod_group(self, node):
        name = new_name = comment = None
        users = []
        actions = {}
        for child in node.findall("*"):
            if child.tag == "GrpName":
                name = child.text.strip()
            elif child.tag == "NewGrpName":
                new_name = child.text.strip()
            elif child.tag == "Comment":
                comment = child.text
            elif child.tag == "UserName":
                users.append(child.text)
            elif child.tag == "Auth":
                action = self.get_node_text(child.find("Action"), "").strip()
                doctype = self.get_node_text(child.find("DocType"), "").strip()
                if not action:
                    raise Exception("Missing Action element in auth")
                if action not in actions:
                    actions[action] = []
                actions[action].append(doctype)
        if not name:
            raise Exception("Missing group name")
        group = self.session.get_group(name)
        if new_name:
            group.name = new_name
        group.comment = comment
        group.users = users
        group.actions = actions
        group.modify(self.session)
        return etree.Element("CdrModGrpResp")

    def _put_link_type(self, node):
        opts = dict(
            sources=[],
            targets={},
            properties=[],
            chk_type=self.get_node_text(node.find("LinkChkType")),
            comment=self.get_node_text(node.find("Comment"))
        )
        name = self.get_node_text(node.find("Name"))
        if node.tag == "CdrModLinkType":
            linktype_id = LinkType(self.session, name=name).id
            if linktype_id is None:
                raise Exception("Can't find link type {}".format(name))
            opts["id"] = linktype_id
            new_name = self.get_node_text(node.find("NewName"))
            opts["name"] = new_name if new_name else name
        else:
            opts["name"] = name
        for wrapper in node.findall("LinkSource"):
            doctype_name = self.get_node_text(wrapper.find("SrcDocType"))
            element = self.get_node_text(wrapper.find("SrcField"))
            doctype = Doctype(self.session, name=doctype_name)
            opts["sources"].append(LinkType.LinkSource(doctype, element))
        for child in node.findall("TargetDocType"):
            doctype_name = self.get_node_text(child)
            doctype = Doctype(self.session, name=doctype_name)
            opts["targets"][doctype.id] = doctype
        message = "Property type {!r} not supported"
        for wrapper in node.findall("LinkProperties"):
            name = self.get_node_text(wrapper.find("LinkProperty"))
            value = self.get_node_text(wrapper.find("PropertyValue"))
            comment = self.get_node_text(wrapper.find("Comment"))
            try:
                cls = getattr(LinkType, name)
                property = cls(self.session, name, value, comment)
                if not isinstance(property, LinkType.Property):
                    raise Exception(message.format(name))
                opts["properties"].append(property)
            except:
                raise Exception(message.format(name))
        linktype = LinkType(self.session, **opts)
        linktype.save()
        return etree.Element(node.tag + "Resp")

    def _put_user(self, node):
        opts = dict(
            name = self.get_node_text(node.find("UserName")),
            fullname=self.get_node_text(node.find("FullName")),
            office=self.get_node_text(node.find("Office")),
            email=self.get_node_text(node.find("Email")),
            phone=self.get_node_text(node.find("Phone")),
            comment=self.get_node_text(node.find("Comment")),
            authmode=self.get_node_text(node.find("AuthenticationMode")),
            groups=[]
        )
        if node.tag == "CdrModUsr":
            opts["id"] = Session.User(self.session, name=opts["name"]).id
            new_name = self.get_node_text(node.find("NewName"))
            if new_name:
                opts["name"] = new_name
        for group in node.findall("GrpName"):
            opts["groups"].append(self.get_node_text(group))
        password = self.get_node_text(node.find("Password"))
        Session.User(self.session, **opts).save(password)
        return etree.Element(node.tag + "Resp")

    def _rep_doc(self, node):
        return self.__put_doc(node, new=False)

    def _rep_filter_set(self, node):
        return self.__put_filter_set(node, new=False)

    def _validate_doc(self, node):
        doctype = node.get("DocType")
        types = node.get("ValidationTypes", "").lower().split()
        opts = {
            "store": "never",
            "types": types or ("schema", "links"),
            "locators": node.get("ErrorLocators") in ("Y", "y")
        }
        xml = doc_id = None
        for child in node:
            if child.tag == "CdrDoc":
                xml = self.get_node_text(child.find("CdrDocXml"))
            elif child.tag == "DocId":
                doc_id = self.get_node_text(child)
                if not child.get("ValidateOnly") != "Y":
                    opts["store"] = "always"
        if not doctype:
            raise Exception("Missing required DocType element")
        if not xml and not doc_id:
            raise Exception("Must specify DocId or CdrDoc element")
        if xml:
            doc = Doc(self.session, xml=xml, doctype=doctype)
        elif doc_id:
            doc = Doc(self.session, id=doc_id)
            if doc.doctype.name != doctype:
                raise Exception("DocType mismatch")
        else:
            raise Exception("Both DocId and CdrDoc specified")
        doc.validate(**opts)
        return doc.legacy_validation_response(opts["locators"])


    #------------------------------------------------------------------
    # HELPER METHODS START HERE.
    #------------------------------------------------------------------

    def __put_doc(self, node, new=False):
        opts = dict()
        doc_opts = dict()
        doc_node = None
        echo = locators = False
        for child in node.findall("*"):
            if child.tag == "CheckIn":
                if self.get_node_text(child, "").upper() == "Y":
                    opts["unlock"] = True
            elif child.tag == "Version":
                if self.get_node_text(child, "").upper() == "Y":
                    opts["version"] = True
                    if child.get("Publishable", "").upper() == "Y":
                        opts["publishable"] = True
            elif child.tag == "Validate":
                if self.get_node_text(child, "").upper() == "Y":
                    val_types = child.get("ValidationTypes", "").lower()
                    if val_types:
                        opts["val_types"] = val_types.split()
                    else:
                        opts["val_types"] = "schema", "links"
                    if child.get("ErrorLocators", "").upper() == "Y":
                        locators = opts["locators"] = True
            elif child.tag == "SetLinks":
                if self.get_node_text(child, "").upper() == "Y":
                    opts["set_links"] = True
            elif child.tag == "Echo":
                if self.get_node_text(child, "").upper() == "Y":
                    echo = True
            elif child.tag == "DelAllBlobVersions":
                if self.get_node_text(child, "").upper() == "Y":
                    opts["del_blobs"] = True
            elif child.tag == "Reason":
                opts["reason"] = self.get_node_text(child)
            elif child.tag == "CdrDoc":
                doc_node = child
        if doc_node is None:
            raise Exception("put_doc() missing CdrDoc element")
        doc_opts = dict(doctype=doc_node.get("Type"))
        if doc_node.get("Id"):
            doc_opts["id"] = doc_node.get("Id")
        for child in doc_node.findall("*"):
            if child.tag == "CdrDocCtl":
                for ctl_node in child.findall("*"):
                    if ctl_node.tag == "DocId" and not doc_opts.get("id"):
                        doc_opts["id"] = self.get_node_text(ctl_node)
                    elif ctl_node.tag == "DocTitle":
                        opts["title"] = self.get_node_text(ctl_node)
                    elif ctl_node.tag == "DocComment":
                        opts["comment"] = self.get_node_text(ctl_node)
                    elif ctl_node.tag == "DocActiveStatus":
                        opts["active_status"] = self.get_node_text(ctl_node)
                    elif ctl_node.tag == "DocNeedsReview":
                        needs_review = self.get_node_text(ctl_node, "N")
                        if needs_review.upper() == "Y":
                            opts["needs_review"] = True
            elif child.tag == "CdrDocXml":
                doc_opts["xml"] = self.get_node_text(child)
            elif child.tag == "CdrDocBlob":
                blob = base64decode(self.get_node_text(child).encode("ascii"))
                doc_opts["blob"] = blob
        if new and doc_opts.get("id"):
            raise Exception("can't add a document which already has an ID")
        if not new and not doc_opts.get("id"):
            raise Exception("CdrRepDoc missing document ID")
        doc = Doc(self.session, **doc_opts)
        doc.save(**opts)
        name = "CdrAddDocResp" if new else "CdrRepDocResp"
        response = etree.Element(name)
        etree.SubElement(response, "DocId").text = doc.cdr_id
        if doc.errors_node:
            response.append(doc.errors_node)
            if doc.is_content_type and opts.get("locators"):
                response.append(doc.legacy_doc(get_xml=True, brief=True))
        return response

    def __put_doctype(self, node, new=False):
        opts = {
            "name": node.get("Type"),
            "format": node.get("Format") or "xml",
            "versioning": node.get("Versioning") or "Y",
            "active": node.get("Active") or "Y"
        }
        for child in node.findall("*"):
            if child.tag == "DocSchema":
                opts["schema"] = self.get_node_text(child)
            elif child.tag == "Comment":
                opts["comment"] = self.get_node_text(child)
        doctype = Doctype(self.session, **opts)
        doctype.save()
        name = "CdrAddDocTypeResp" if new else "CdrModDocTypeResp"
        return etree.Element(name)

    def __load_commands(self):
        content_length = os.environ.get("CONTENT_LENGTH")
        if content_length:
            request = sys.stdin.buffer.read(int(content_length))
        else:
            request = sys.stdin.buffer.read()
        self.logger.info("%s bytes from %s", len(request), self.client)
        root = etree.fromstring(request, parser=self.PARSER)
        if root.tag != "CdrCommandSet":
            raise Exception("not a CDR command set")
        return root

    def __process_command(self, node):
        start = datetime.datetime.now()
        response = etree.Element("CdrResponse")
        command_id = node.get("CmdId")
        if command_id:
            response.set("CmdId", command_id)
        try:
            if not self.session:
                raise Exception("Missing session ID")
            child = node.find("*")
            if child is None:
                raise Exception("Missing specific command element")
            self.logger.info(child.tag)
            handler = self.COMMANDS.get(child.tag)
            if handler is None:
                raise Exception("Unknown command: {}".format(child.tag))
            response.append(getattr(self, handler)(child))
            response.set("Status", "success")
        except Exception as e:
            self.logger.exception("{} failure".format(node.tag))
            response.append(self.__wrap_error(e))
            response.set("Status", "failure")
        elapsed = (datetime.datetime.now() - start).total_seconds()
        response.set("Elapsed", "{:f}".format(elapsed))
        return response

    def __put_filter_set(self, node, new):
        opts = dict(name=None, description=None, notes=None, members=[])
        for child in node:
            if child.tag == "FilterSetName":
                opts["name"] = self.get_node_text(child)
            elif child.tag == "FilterSetDescription":
                opts["description"] = self.get_node_text(child)
            elif child.tag == "FilterSetNotes":
                opts["notes"] = self.get_node_text(child)
            elif child.tag == "Filter":
                member = Doc(self.session, id=child.get("DocId"))
                opts["members"].append(member)
            elif child.tag == "FilterSet":
                member = FilterSet(self.session, id=child.get("SetId"))
                opts["members"].append(member)
        filter_set = FilterSet(self.session, **opts)
        if new and filter_set.id:
            message = "Filter set {!r} already exists".format(filter_set.name)
            raise Exception(message)
        if not new and not filter_set.id:
            message = "Filter set {!r} not found".format(filter_set.name)
            raise Exception(message)
        member_count = filter_set.save()
        name = "CdrAddFilterSetResp" if new else "CdrRepFilterSetResp"
        return etree.Element(name, TotalFilters=str(member_count))

    def __wrap_error(self, error):
        errors = etree.Element("Errors")
        etree.SubElement(errors, "Err").text = str(error)
        return errors

    def __wrap_responses(self, *responses):
        response_set = etree.Element("CdrResponseSet")
        response_set.set("Time", self.start.strftime("%Y-%m-%dT%H:%M:%S.%f"))
        for response in responses:
            response_set.append(response)
        return etree.tostring(response_set, encoding="utf-8")

    @staticmethod
    def get_node_text(node, default=None):
        if node is None:
            return default
        return "".join(node.itertext("*"))
