/*
 * $Id: CdrXsd.cpp,v 1.1 2000-04-11 14:15:56 bkline Exp $
 *
 * $Log: not supported by cvs2svn $
 */

// System headers.
#include <climits>

// Project headers.
#include "CdrXsd.h"

/**
 * Extracts document constraint information from the schema document.
 */
cdr::xsd::Schema::Schema(const cdr::dom::Node& schemaElement)
{
    topElement = 0;
    seedBuiltinTypes();
    cdr::String nodeName = schemaElement.getNodeName();
    if (nodeName != cdr::xsd::SCHEMA)
        throw cdr::Exception(L"Top-level element must be xsd:schema");
    cdr::dom::Node childNode = schemaElement.getFirstChild();
    while (childNode != 0) {
        if (childNode.getNodeType() == cdr::dom::Node::ELEMENT_NODE) {
            nodeName = childNode.getNodeName();
            if (nodeName == cdr::xsd::ELEMENT) {
                if (topElement)
                    throw cdr::Exception(
                        L"Only one top-level element allowed in schema");
                topElement = new cdr::xsd::Element(childNode);
            }
            else if (nodeName == cdr::xsd::COMPLEX_TYPE)
                registerType(new cdr::xsd::ComplexType(childNode));
            else if (nodeName == cdr::xsd::SIMPLE_TYPE)
                registerType(new cdr::xsd::SimpleType(childNode));
        }
        childNode = childNode.getNextSibling();
    }
}

/**
 * Initializes the type collection with the builtin simple types.
 * XXX This is a stub.  Needs real knowledge of what the semantics
 * are for these types.
 */
void cdr::xsd::Schema::seedBuiltinTypes()
{
    registerType(new cdr::xsd::SimpleType(cdr::xsd::STRING));
    registerType(new cdr::xsd::SimpleType(cdr::xsd::DATE));
    registerType(new cdr::xsd::SimpleType(cdr::xsd::TIME));
    registerType(new cdr::xsd::SimpleType(cdr::xsd::DECIMAL));
    registerType(new cdr::xsd::SimpleType(cdr::xsd::INTEGER));
    registerType(new cdr::xsd::SimpleType(cdr::xsd::BINARY));
    registerType(new cdr::xsd::SimpleType(cdr::xsd::URI));
    registerType(new cdr::xsd::SimpleType(cdr::xsd::TIME_INSTANT));
}

/**
 * Adds a new type to the map of types for the schema, first checking
 * for duplicate type names.
 */
void cdr::xsd::Schema::registerType(const cdr::xsd::Type* type)
{
    cdr::String name = type->getName();
    if (types.find(name) != types.end())
        throw cdr::Exception(L"Duplicate type name", name);
    types[name] = type;
}

/**
 * Finds a type by name in the schema's type collection.
 */
const cdr::xsd::Type* cdr::xsd::Schema::lookupType(const cdr::String& typeName) const
{
    TypeMap::const_iterator i = types.find(typeName);
    if (i != types.end())
        return i->second;
    return 0;
}

/**
 * Cleans up contents of schema type collection and drops top element.
 */
cdr::xsd::Schema::~Schema()
{
    for (TypeMap::iterator i = types.begin(); i != types.end(); ++i)
        delete i->second;
    delete topElement;
}

/**
 * Finds this node's type in the collection of types.  Caches the answer
 * so it doesn't have to do the search more than once.
 */
const cdr::xsd::Type* cdr::xsd::Node::resolveType(const cdr::xsd::Schema& schema)
{
    if (!type)
        type = schema.lookupType(typeName);
    return type;
}

/**
 * Builds a new schema element object from its XML node in the schema
 * document.
 */
cdr::xsd::Element::Element(const cdr::dom::Node& dn)
{
    maxOccs = minOccs = 1;
    cdr::dom::NamedNodeMap attrs = dn.getAttributes();
    int nAttrs = attrs.getLength();
    for (int i = 0; i < nAttrs; ++i) {
        cdr::dom::Node attr = attrs.item(i);
        cdr::String attrName = attr.getNodeName();
        cdr::String attrValue = attr.getNodeValue();
        if (attrName == cdr::xsd::NAME)
            name = attrValue;
        else if (attrName == cdr::xsd::TYPE)
            typeName = attrValue;
        else if (attrName == cdr::xsd::MIN_OCCURS)
            minOccs = attrValue.getInt();
        else if (attrName == cdr::xsd::MAX_OCCURS) {
            if (attrValue == cdr::xsd::UNLIMITED)
                maxOccs = INT_MAX;
            else
                maxOccs = attrValue.getInt();
        }
    }
    if (name.size() == 0)
        throw cdr::Exception(L"Name missing for element");
    if (typeName.size() == 0)
        throw cdr::Exception(L"Type missing for element", name);
}

/**
 * Builds a new schema element object from the xsd:attribute node in the 
 * schema document.
 */
cdr::xsd::Attribute::Attribute(const cdr::dom::Node& dn)
{
    cdr::dom::NamedNodeMap attrs = dn.getAttributes();
    int nAttrs = attrs.getLength();
    for (int i = 0; i < nAttrs; ++i) {
        cdr::dom::Node attr = attrs.item(i);
        cdr::String attrName = attr.getNodeName();
        cdr::String attrValue = attr.getNodeValue();
        if (attrName == cdr::xsd::NAME)
            name = attrValue;
        else if (attrName == cdr::xsd::TYPE)
            typeName = attrValue;
    }
    if (name.size() == 0)
        throw cdr::Exception(L"Name missing for attribute");
    if (typeName.size() == 0)
        throw cdr::Exception(L"Type missing for attribute", name);
}

/**
 * Builds a new schema element object from the xsd:simpleType node in the 
 * schema document.
 */
cdr::xsd::SimpleType::SimpleType(const cdr::dom::Node& dn)
{
    // Not using these yet.
    base = 0;
    minLength = 0;
    maxLength = INT_MAX;
    length = -1;
    precision = -1;
    scale = -1;
    encoding = HEX;

    cdr::dom::NamedNodeMap attrs = dn.getAttributes();
    int nAttrs = attrs.getLength();
    for (int i = 0; i < nAttrs; ++i) {
        cdr::dom::Node attr = attrs.item(i);
        cdr::String attrName = attr.getNodeName();
        cdr::String attrValue = attr.getNodeValue();
        if (attrName == cdr::xsd::NAME)
            setName(attrValue);
        else if (attrName == cdr::xsd::BASE)
            baseName = attrValue;
    }
    if (getName().size() == 0)
        throw cdr::Exception(L"Name missing for simple type");
    cdr::dom::Node childNode = dn.getFirstChild();
    while (childNode != 0) {
        int nodeType = childNode.getNodeType();
        if (nodeType == cdr::dom::Node::ELEMENT_NODE) {
            cdr::String nodeName = childNode.getNodeName();
            cdr::String nodeValue = childNode.getNodeValue();
            if (nodeName == cdr::xsd::ENUMERATION)
                enumSet.insert(nodeValue);
            else if (nodeName == cdr::xsd::MIN_INCLUSIVE)
                minInclusive = nodeValue;
            else if (nodeName == cdr::xsd::MAX_INCLUSIVE)
                maxInclusive = nodeValue;
            else if (nodeName == cdr::xsd::MIN_LENGTH)
                minLength = nodeValue.getInt();
            else if (nodeName == cdr::xsd::MAX_LENGTH)
                maxLength = nodeValue.getInt();
            else if (nodeName == cdr::xsd::LENGTH)
                length = nodeValue.getInt();
            else if (nodeName == cdr::xsd::PATTERN)
                patterns.push_back(nodeValue);
            else if (nodeName == cdr::xsd::PRECISION)
                precision = nodeValue.getInt();
            else if (nodeName == cdr::xsd::SCALE)
                scale = nodeValue.getInt();
            else if (nodeName == cdr::xsd::ENCODING) {
                if (nodeValue == cdr::xsd::HEX)
                    encoding = HEX;
                else if (nodeValue == cdr::xsd::BASE64)
                    encoding = BASE64;
                else
                    throw cdr::Exception(L"Illegal encoding value", nodeValue);
            }
        }
        childNode = childNode.getNextSibling();
    }
}

/**
 * Builds a new schema element object from the xsd:complexType node in the 
 * schema document.
 */
cdr::xsd::ComplexType::ComplexType(const cdr::dom::Node& dn)
{
    contentType = ELEMENT_ONLY;
    cdr::dom::NamedNodeMap attrs = dn.getAttributes();
    int nAttrs = attrs.getLength();
    for (int i = 0; i < nAttrs; ++i) {
        cdr::dom::Node attr = attrs.item(i);
        cdr::String attrName = attr.getNodeName();
        cdr::String attrValue = attr.getNodeValue();
        if (attrName == cdr::xsd::NAME)
            setName(attrValue);
        else if (attrName == cdr::xsd::CONTENT) {
            if (attrValue == cdr::xsd::TEXT_ONLY)
                contentType = TEXT_ONLY;
            else if (attrValue == cdr::xsd::EMPTY)
                contentType = EMPTY;
            else if (attrValue == cdr::xsd::MIXED)
                contentType = MIXED;
        }
    }
    cdr::dom::Node childNode = dn.getFirstChild();
    while (childNode != 0) {
        int nodeType = childNode.getNodeType();
        if (nodeType == cdr::dom::Node::ELEMENT_NODE) {
            cdr::String nodeName = childNode.getNodeName();
            if (nodeName == cdr::xsd::ELEMENT)
                elementList.push_back(new cdr::xsd::Element(childNode));
            else if (nodeName == cdr::xsd::ATTRIBUTE)
                attributeList.push_back(new cdr::xsd::Attribute(childNode));
        }
        childNode = childNode.getNextSibling();
    }
    if (getName().size() == 0)
        throw cdr::Exception(L"Name missing for complex type");
}

/**
 * Cleans up the lists of elements and attributes which make up this type.
 */
cdr::xsd::ComplexType::~ComplexType()
{
    for (cdr::xsd::ElementList::iterator eli = elementList.begin(); 
         eli != elementList.end(); 
         ++eli)
        delete *eli;
    for (cdr::xsd::AttributeList::iterator ali = attributeList.begin(); 
         ali != attributeList.end(); 
         ++ali)
        delete *ali;
}
