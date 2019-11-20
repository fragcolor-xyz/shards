/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */
// Code Derived from https://github.com/shaunxcode/edn-cpp
// Copyright (c) 2013 shaun gilchrist

#ifndef CB_EDN_HPP
#define CB_EDN_HPP

#include <list>
#include <string>
#include <cstring>
#include <iostream>
#include <algorithm>
namespace edn { 
  using std::cout;
  using std::endl;
  using std::string;
  using std::list;

  string validSymbolChars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.*+!-_?$%&=:#/";
  enum TokenType {
    TokenString,
    TokenAtom,
    TokenParen
  };

  struct EdnToken {
    TokenType type;
    int line;
    string value;
  };

  enum NodeType {
    EdnNil,
    EdnSymbol,
    EdnKeyword,
    EdnBool,
    EdnInt,
    EdnFloat,
    EdnString, 
    EdnChar, 

    EdnList,
    EdnVector,
    EdnMap,
    EdnSet,

    EdnDiscard,
    EdnTagged
  };

  struct EdnNode {
    NodeType type;
    int line;
    string value;
    list<EdnNode> values;
  };

  void createToken(TokenType type, int line, string value, list<EdnToken> &tokens) {
    EdnToken token;
    token.type = type;
    token.line = line;
    token.value = value;
    tokens.push_back(token);
  }

  string typeToString(NodeType type) { 
    string output;
    switch (type) { 
      case EdnSymbol:  output = "EdnSymbol";   break;
      case EdnKeyword: output = "EdnKeyword";  break;
      case EdnInt:     output = "EdnInt";      break;
      case EdnFloat:   output = "EdnFloat";    break;
      case EdnChar:    output = "EdnChar";     break;
      case EdnBool:    output = "EdnBool";     break;
      case EdnNil:     output = "EdnNil";      break;
      case EdnString:  output = "EdnString";   break;
      case EdnTagged:  output = "EdnTagged";   break;
      case EdnList:    output = "EdnList";     break;
      case EdnVector:  output = "EdnVector";   break;
      case EdnSet:     output = "EdnSet";      break;
      case EdnMap:     output = "EdnMap";      break;
      case EdnDiscard: output = "EdnDiscard";  break;
    }
    return output;
  }

  //by default checks if first char is in range of chars
  bool strRangeIn(string str, const char* range, int start = 0, int stop = 1) {
    string strRange = str.substr(start, stop);
    return (std::strspn(strRange.c_str(), range) == strRange.length());
  }

  list<EdnToken> lex(string edn) {
    string::iterator it;
    int line = 1;
    char escapeChar = '\\';
    bool escaping = false;
    bool inString = false;
    string stringContent = "";
    bool inComment = false;
    string token = "";
    string paren = "";
    list<EdnToken> tokens;

    for (it = edn.begin(); it != edn.end(); ++it) {
      if (*it == '\n' || *it == '\r') line++;

      if (!inString && *it == ';' && !escaping) inComment = true;

      if (inComment) {
        if (*it == '\n') {
          inComment = false;
          if (token != "") {
              createToken(TokenAtom, line, token, tokens);
              token = "";
          }
          continue;
        }
      }
    
      if (*it == '"' && !escaping) {
        if (inString) {
          createToken(TokenString, line, stringContent, tokens);
          inString = false;
        } else {
          stringContent = "";
          inString = true;
        }
        continue;
      }

      if (inString) { 
        if (*it == escapeChar && !escaping) {
          escaping = true;
          continue;
        }

        if (escaping) {
          escaping = false;
          if (*it == 't' || *it == 'n' || *it == 'f' || *it == 'r') stringContent += escapeChar;
        }
        stringContent += *it;   
      } else if (*it == '(' || *it == ')' || *it == '[' || *it == ']' || *it == '{' || 
                 *it == '}' || *it == '\t' || *it == '\n' || *it == '\r' || *it == ' ' || *it == ',') {
        if (token != "") { 
          createToken(TokenAtom, line, token, tokens);
          token = "";
        } 
        if (*it == '(' || *it == ')' || *it == '[' || *it == ']' || *it == '{' || *it == '}') {
          paren = "";
          paren += *it;
          createToken(TokenParen, line, paren, tokens);
        }
      } else {
        if (escaping) { 
          escaping = false;
        } else if (*it == escapeChar) {
          escaping = true;
        }

        if (token == "#_" || (token.length() == 2 && token[0] == escapeChar)) {
          createToken(TokenAtom, line, token, tokens); 
          token = "";
        }
        token += *it;
      }
    }
    if (token != "") {
      createToken(TokenAtom, line, token, tokens); 
    }

    return tokens;
  }

  void uppercase(string &str) { 
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
  }

  bool validSymbol(string value) {
    //first we uppercase the value
    uppercase(value);

    if (std::strspn(value.c_str(), validSymbolChars.c_str()) != value.length())
      return false;
    
    //if the value starts with a number that is not ok
    if (strRangeIn(value, "0123456789"))
      return false;

    //first char can not start with : # or / - but / by itself is valid
    if (strRangeIn(value, ":#/") && !(value.length() == 1 && value[0] == '/'))
      return false;

    //if the first car is - + or . then the next char must NOT be numeric, by by themselves they are valid
    if (strRangeIn(value, "-+.") && value.length() > 1 && strRangeIn(value, "0123456789", 1))
      return false;

    if (std::count(value.begin(), value.end(), '/') > 1)
      return false;

    return true;
  }

  bool validKeyword(string value) { 
    return (value[0] == ':' && validSymbol(value.substr(1,value.length() - 1)));
  }

  bool validNil(string value) {
    return (value == "nil");
  }

  bool validBool(string value) {
    return (value == "true" || value == "false");
  }

  bool validInt(string value, bool allowSign = true) {
    //if we have a positive or negative symbol that is ok but remove it for testing
    if (strRangeIn(value, "-+") && value.length() > 1 && allowSign) 
      value = value.substr(1, value.length() - 1); 

    //if string ends with N or M that is ok, but remove it for testing
    if (strRangeIn(value, "NM", value.length() - 1, 1))
      value = value.substr(0, value.length() - 2); 

    if (std::strspn(value.c_str(), "0123456789") != value.length())
      return false;

    return true;
  }

  bool validFloat(string value) {
    uppercase(value); 

    string front;
    string back;
    int epos;
    int periodPos = value.find_first_of('.');
    if (periodPos) {
      front = value.substr(0, periodPos);
      back = value.substr(periodPos + 1);
    } else {
      front = "";
      back = value;
    }
    
    if(front == "" || validInt(front)) { 
      epos = back.find_first_of('E');
      if(epos > -1) { 
        //ends with E which is invalid
        if ((unsigned)epos == back.length() - 1) return false;

        //both the decimal and exponent should be valid - do not allow + or - on dec (pass false as arg to validInt)
        if (!validInt(back.substr(0, epos), false) || !validInt(back.substr(epos + 1))) return false;
      } else { 
        //if back ends with M remove for validation
        if (strRangeIn(back, "M", back.length() - 1, 1)) 
          back = back.substr(0, back.length() - 1);
     
        if (!validInt(back, false)) return false;
      }
      return true; 
    }
  
    return false;
  }

  bool validChar(string value) {
    return (value.at(0) == '\\' && value.length() == 2);
  }

  EdnNode handleAtom(EdnToken token) {
    EdnNode node;
    node.line = token.line;
    node.value = token.value;

    if (validNil(token.value))
      node.type = EdnNil;
    else if (token.type == TokenString) 
      node.type = EdnString;
    else if (validChar(token.value))
      node.type = EdnChar;
    else if (validBool(token.value))
      node.type = EdnBool;
    else if (validInt(token.value))
      node.type = EdnInt;
    else if (validFloat(token.value))
      node.type = EdnFloat;
    else if (validKeyword(token.value))
      node.type = EdnKeyword;
    else if (validSymbol(token.value))
      node.type = EdnSymbol;
    else
      throw "Could not parse atom";

    return node;
  }

  EdnNode handleCollection(EdnToken token, list<EdnNode> values) {
    EdnNode node;
    node.line = token.line;
    node.values = values;

    if (token.value == "(") {
      node.type = EdnList;
    }
    else if (token.value == "[") {
      node.type = EdnVector;
    }
    if (token.value == "{") {
      node.type = EdnMap;
    }
    return node;
  }

  EdnNode handleTagged(EdnToken token, EdnNode value) { 
    EdnNode node;
    node.line = token.line;

    string tagName = token.value.substr(1, token.value.length() - 1);
    if (tagName == "_") {
      node.type = EdnDiscard;
    } else if (tagName == "") {
      //special case where we return early as # { is a set - thus tagname is empty
      node.type = EdnSet;
      if (value.type != EdnMap) {
        throw "Was expection a { } after hash to build set";
      }
      node.values = value.values;
      return node;
    } else {
      node.type = EdnTagged;
    }
  
    if (!validSymbol(tagName)) {
      throw "Invalid tag name"; 
    }

    EdnToken symToken;
    symToken.type = TokenAtom;
    symToken.line = token.line;
    symToken.value = tagName;
  
    list<EdnNode> values;
    values.push_back(handleAtom(symToken)); 
    values.push_back(value);

    node.values = values;
    return node; 
  }

  EdnToken shiftToken(list<EdnToken> &tokens) { 
    EdnToken nextToken = tokens.front();
    tokens.pop_front();
    return nextToken;
  }

  EdnNode readAhead(EdnToken token, list<EdnToken> &tokens) {
    if (token.type == TokenParen) {

      EdnToken nextToken;
      list<EdnNode> L;
      string closeParen;
      if (token.value == "(") closeParen = ")";
      if (token.value == "[") closeParen = "]"; 
      if (token.value == "{") closeParen = "}"; 

      while (true) {
        if (tokens.empty()) throw "unexpected end of list";

        nextToken = shiftToken(tokens);

        if (nextToken.value == closeParen) {
          return handleCollection(token, L);
        } else {
          L.push_back(readAhead(nextToken, tokens));
        }
      }
    } else if (token.value == ")" || token.value == "]" || token.value == "}") {
      throw "Unexpected " + token.value;
    } else {
      if (token.value.size() && token.value.at(0) == '#') {
        return handleTagged(token, readAhead(shiftToken(tokens), tokens));
      } else {
        return handleAtom(token);
      }
    }
  }

  string escapeQuotes(const string &before) {
    string after;
    after.reserve(before.length() + 4); 
    
    for (string::size_type i = 0; i < before.length(); ++i) { 
      switch (before[i]) { 
        case '"':
        case '\\':
          after += '\\';
        default:
          after += before[i];
      }
    }
    return after;
  }

  string pprint(EdnNode &node, int indent = 1) {
    string prefix("");
    if (indent) {
      prefix.insert(0, indent, ' ');
    }

    string output;
    if (node.type == EdnList || node.type == EdnSet || node.type == EdnVector || node.type == EdnMap) { 
      string vals = "";
      for (list<EdnNode>::iterator it=node.values.begin(); it != node.values.end(); ++it) {
        if (vals.length() > 0) vals += prefix;
        vals += pprint(*it, indent + 1);
        if (node.type == EdnMap) { 
          ++it;
          vals += " " + pprint(*it, 1);
        }
        if (std::distance(it, node.values.end()) != 1) vals += "\n";
      }

      if (node.type == EdnList) output = "(" + vals + ")";
      else if (node.type == EdnVector) output = "[" + vals + "]";
      else if (node.type == EdnMap) output = "{" + vals + "}"; 
      else if (node.type == EdnSet) output = "#{" + vals + "}";
     
      #ifdef DEBUG
        return "<" + typeToString(node.type) + " " + output + ">"; 
      #endif 
    } else if (node.type == EdnTagged) {
      output = "#" + pprint(node.values.front()) + " " + pprint(node.values.back());
      #ifdef DEBUG
        return "<EdnTagged " + output + ">";
      #endif
    } else if (node.type == EdnString) {
      output = "\"" + escapeQuotes(node.value) + "\"";
      #ifdef DEBUG
        return "<EdnString " + output + ">";
      #endif
    } else {
      #ifdef DEBUG
        return "<" + typeToString(node.type) + " " + node.value + ">";
      #endif

      output = node.value;
    }
    return output;
  }

  EdnNode read(string edn) {
    list<EdnToken> tokens = lex(edn);
    
    if (tokens.size() == 0) {
      throw "No parsable tokens found in string";
    }

    return readAhead(shiftToken(tokens), tokens);
  }
}

#endif
