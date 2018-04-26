//
//Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
//Copyright (C) 2012-2013 LunarG, Inc.
//
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions
//are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//POSSIBILITY OF SUCH DAMAGE.
//

//
// This header defines a two-level parse-helper hierarchy, derived from
// TParseVersions:
//  - TParseContextBase:  sharable across multiple parsers
//  - TParseContext:      GLSL specific helper
//

#ifndef _PARSER_HELPER_INCLUDED_
#define _PARSER_HELPER_INCLUDED_

#include "parseVersions.h"
#include "../Include/ShHandle.h"
#include "SymbolTable.h"
#include "localintermediate.h"
#include "Scan.h"
#include <functional>

#include <cstdarg>

namespace glslang {

struct TPragma {
    TPragma(bool o, bool d) : optimize(o), debug(d) { }
    bool optimize;
    bool debug;
    TPragmaTable pragmaTable;
};

class TScanContext;
class TPpContext;

typedef std::set<int> TIdSetType;

//
// Sharable code (as well as what's in TParseVersions) across
// parse helpers.
//
class TParseContextBase : public TParseVersions {
public:
    TParseContextBase(TSymbolTable& symbolTable, TIntermediate& interm, int version,
                      EProfile profile, const SpvVersion& spvVersion, EShLanguage language,
                      TInfoSink& infoSink, bool forwardCompatible, EShMessages messages)
          : TParseVersions(interm, version, profile, spvVersion, language, infoSink, forwardCompatible, messages),
            symbolTable(symbolTable), tokensBeforeEOF(false),
            linkage(nullptr), scanContext(nullptr), ppContext(nullptr) { }
    virtual ~TParseContextBase() { }
    
    virtual void setLimits(const TBuiltInResource&) = 0;
    
    EShLanguage getLanguage() const { return language; }
    TIntermAggregate*& getLinkage() { return linkage; }
    void setScanContext(TScanContext* c) { scanContext = c; }
    TScanContext* getScanContext() const { return scanContext; }
    void setPpContext(TPpContext* c) { ppContext = c; }
    TPpContext* getPpContext() const { return ppContext; }

    virtual void setLineCallback(const std::function<void(int, int, bool, int, const char*)>& func) { lineCallback = func; }
    virtual void setExtensionCallback(const std::function<void(int, const char*, const char*)>& func) { extensionCallback = func; }
    virtual void setVersionCallback(const std::function<void(int, int, const char*)>& func) { versionCallback = func; }
    virtual void setPragmaCallback(const std::function<void(int, const TVector<TString>&)>& func) { pragmaCallback = func; }
    virtual void setErrorCallback(const std::function<void(int, const char*)>& func) { errorCallback = func; }

    virtual void reservedPpErrorCheck(const TSourceLoc&, const char* name, const char* op) = 0;
    virtual bool lineContinuationCheck(const TSourceLoc&, bool endOfComment) = 0;
    virtual bool lineDirectiveShouldSetNextLine() const = 0;
    virtual void handlePragma(const TSourceLoc&, const TVector<TString>&) = 0;

    virtual bool parseShaderStrings(TPpContext&, TInputScanner& input, bool versionWillBeError = false) = 0;

    virtual void notifyVersion(int line, int version, const char* type_string)
    {
        if (versionCallback)
            versionCallback(line, version, type_string);
    }
    virtual void notifyErrorDirective(int line, const char* error_message)
    {
        if (errorCallback)
            errorCallback(line, error_message);
    }
    virtual void notifyLineDirective(int curLineNo, int newLineNo, bool hasSource, int sourceNum, const char* sourceName)
    {
        if (lineCallback)
            lineCallback(curLineNo, newLineNo, hasSource, sourceNum, sourceName);
    }
    virtual void notifyExtensionDirective(int line, const char* extension, const char* behavior)
    {
        if (extensionCallback)
            extensionCallback(line, extension, behavior);
    }

    TSymbolTable& symbolTable;   // symbol table that goes with the current language, version, and profile
    bool tokensBeforeEOF;

protected:
    TParseContextBase(TParseContextBase&);
    TParseContextBase& operator=(TParseContextBase&);

    TIntermAggregate* linkage;   // aggregate node of objects the linker may need, if not referenced by the rest of the AST
    TScanContext* scanContext;
    TPpContext* ppContext;

    // These, if set, will be called when a line, pragma ... is preprocessed.
    // They will be called with any parameters to the original directive.
    std::function<void(int, int, bool, int, const char*)> lineCallback;
    std::function<void(int, const TVector<TString>&)> pragmaCallback;
    std::function<void(int, int, const char*)> versionCallback;
    std::function<void(int, const char*, const char*)> extensionCallback;
    std::function<void(int, const char*)> errorCallback;
};

//
// GLSL-specific parse helper.  Should have GLSL in the name, but that's
// too big of a change for comparing branches at the moment, and perhaps
// impacts downstream consumers as well.
//
class TParseContext : public TParseContextBase {
public:
    TParseContext(TSymbolTable&, TIntermediate&, bool parsingBuiltins, int version, EProfile, const SpvVersion& spvVersion, EShLanguage, TInfoSink&,
                  bool forwardCompatible = false, EShMessages messages = EShMsgDefault);
    virtual ~TParseContext();

    void setLimits(const TBuiltInResource&);
    bool parseShaderStrings(TPpContext&, TInputScanner& input, bool versionWillBeError = false);
    void parserError(const char* s);     // for bison's yyerror

    void C_DECL error(const TSourceLoc&, const char* szReason, const char* szToken,
                      const char* szExtraInfoFormat, ...);
    void C_DECL  warn(const TSourceLoc&, const char* szReason, const char* szToken,
                      const char* szExtraInfoFormat, ...);
    void C_DECL ppError(const TSourceLoc&, const char* szReason, const char* szToken,
                      const char* szExtraInfoFormat, ...);
    void C_DECL ppWarn(const TSourceLoc&, const char* szReason, const char* szToken,
                      const char* szExtraInfoFormat, ...);

    void reservedErrorCheck(const TSourceLoc&, const TString&);
    void reservedPpErrorCheck(const TSourceLoc&, const char* name, const char* op);
    bool lineContinuationCheck(const TSourceLoc&, bool endOfComment);
    bool lineDirectiveShouldSetNextLine() const;
    bool builtInName(const TString&);

    void handlePragma(const TSourceLoc&, const TVector<TString>&);
    TIntermTyped* handleVariable(const TSourceLoc&, TSymbol* symbol, const TString* string);
    TIntermTyped* handleBracketDereference(const TSourceLoc&, TIntermTyped* base, TIntermTyped* index);
    void checkIndex(const TSourceLoc&, const TType&, int& index);
    void handleIndexLimits(const TSourceLoc&, TIntermTyped* base, TIntermTyped* index);

    void makeEditable(TSymbol*&);
    TVariable* getEditableVariable(const char* name);
    bool isIoResizeArray(const TType&) const;
    void fixIoArraySize(const TSourceLoc&, TType&);
    void ioArrayCheck(const TSourceLoc&, const TType&, const TString& identifier);
    void handleIoResizeArrayAccess(const TSourceLoc&, TIntermTyped* base);
    void checkIoArraysConsistency(const TSourceLoc&, bool tailOnly = false);
    int getIoArrayImplicitSize() const;
    void checkIoArrayConsistency(const TSourceLoc&, int requiredSize, const char* feature, TType&, const TString&);

    TIntermTyped* handleBinaryMath(const TSourceLoc&, const char* str, TOperator op, TIntermTyped* left, TIntermTyped* right);
    TIntermTyped* handleUnaryMath(const TSourceLoc&, const char* str, TOperator op, TIntermTyped* childNode);
    TIntermTyped* handleDotDereference(const TSourceLoc&, TIntermTyped* base, const TString& field);
    void blockMemberExtensionCheck(const TSourceLoc&, const TIntermTyped* base, const TString& field);
    TFunction* handleFunctionDeclarator(const TSourceLoc&, TFunction& function, bool prototype);
    TIntermAggregate* handleFunctionDefinition(const TSourceLoc&, TFunction&);
    TIntermTyped* handleFunctionCall(const TSourceLoc&, TFunction*, TIntermNode*);
    TIntermNode* handleReturnValue(const TSourceLoc&, TIntermTyped*);
    void checkLocation(const TSourceLoc&, TOperator);
    TIntermTyped* handleLengthMethod(const TSourceLoc&, TFunction*, TIntermNode*);
    void addInputArgumentConversions(const TFunction&, TIntermNode*&) const;
    TIntermTyped* addOutputArgumentConversions(const TFunction&, TIntermAggregate&) const;
    void builtInOpCheck(const TSourceLoc&, const TFunction&, TIntermOperator&);
    void nonOpBuiltInCheck(const TSourceLoc&, const TFunction&, TIntermAggregate&);
    TFunction* handleConstructorCall(const TSourceLoc&, const TPublicType&);

    bool parseVectorFields(const TSourceLoc&, const TString&, int vecSize, TVectorFields&);
    void assignError(const TSourceLoc&, const char* op, TString left, TString right);
    void unaryOpError(const TSourceLoc&, const char* op, TString operand);
    void binaryOpError(const TSourceLoc&, const char* op, TString left, TString right);
    void variableCheck(TIntermTyped*& nodePtr);
    bool lValueErrorCheck(const TSourceLoc&, const char* op, TIntermTyped*);
    void rValueErrorCheck(const TSourceLoc&, const char* op, TIntermTyped*);
    void constantValueCheck(TIntermTyped* node, const char* token);
    void integerCheck(const TIntermTyped* node, const char* token);
    void globalCheck(const TSourceLoc&, const char* token);
    bool constructorError(const TSourceLoc&, TIntermNode*, TFunction&, TOperator, TType&);
    bool constructorTextureSamplerError(const TSourceLoc&, const TFunction&);
    void arraySizeCheck(const TSourceLoc&, TIntermTyped* expr, TArraySize&);
    bool arrayQualifierError(const TSourceLoc&, const TQualifier&);
    bool arrayError(const TSourceLoc&, const TType&);
    void arraySizeRequiredCheck(const TSourceLoc&, const TArraySizes&);
    void structArrayCheck(const TSourceLoc&, const TType& structure);
    void arrayUnsizedCheck(const TSourceLoc&, const TQualifier&, const TArraySizes*, bool initializer, bool lastMember);
    void arrayOfArrayVersionCheck(const TSourceLoc&);
    void arrayDimCheck(const TSourceLoc&, const TArraySizes* sizes1, const TArraySizes* sizes2);
    void arrayDimCheck(const TSourceLoc&, const TType*, const TArraySizes*);
    void arrayDimMerge(TType& type, const TArraySizes* sizes);
    bool voidErrorCheck(const TSourceLoc&, const TString&, TBasicType);
    void boolCheck(const TSourceLoc&, const TIntermTyped*);
    void boolCheck(const TSourceLoc&, const TPublicType&);
    void samplerCheck(const TSourceLoc&, const TType&, const TString& identifier, TIntermTyped* initializer);
    void atomicUintCheck(const TSourceLoc&, const TType&, const TString& identifier);
    void transparentCheck(const TSourceLoc&, const TType&, const TString& identifier);
    void globalQualifierFixCheck(const TSourceLoc&, TQualifier&);
    void globalQualifierTypeCheck(const TSourceLoc&, const TQualifier&, const TPublicType&);
    bool structQualifierErrorCheck(const TSourceLoc&, const TPublicType& pType);
    void mergeQualifiers(const TSourceLoc&, TQualifier& dst, const TQualifier& src, bool force);
    void setDefaultPrecision(const TSourceLoc&, TPublicType&, TPrecisionQualifier);
    int computeSamplerTypeIndex(TSampler&);
    TPrecisionQualifier getDefaultPrecision(TPublicType&);
    void precisionQualifierCheck(const TSourceLoc&, TBasicType, TQualifier&);
    void parameterTypeCheck(const TSourceLoc&, TStorageQualifier qualifier, const TType& type);
    bool containsFieldWithBasicType(const TType& type ,TBasicType basicType);
    TSymbol* redeclareBuiltinVariable(const TSourceLoc&, const TString&, const TQualifier&, const TShaderQualifiers&, bool& newDeclaration);
    void redeclareBuiltinBlock(const TSourceLoc&, TTypeList& typeList, const TString& blockName, const TString* instanceName, TArraySizes* arraySizes);
    void paramCheckFix(const TSourceLoc&, const TStorageQualifier&, TType& type);
    void paramCheckFix(const TSourceLoc&, const TQualifier&, TType& type);
    void nestedBlockCheck(const TSourceLoc&);
    void nestedStructCheck(const TSourceLoc&);
    void arrayObjectCheck(const TSourceLoc&, const TType&, const char* op);
    void opaqueCheck(const TSourceLoc&, const TType&, const char* op);
    void specializationCheck(const TSourceLoc&, const TType&, const char* op);
    void structTypeCheck(const TSourceLoc&, TPublicType&);
    void inductiveLoopCheck(const TSourceLoc&, TIntermNode* init, TIntermLoop* loop);
    void arrayLimitCheck(const TSourceLoc&, const TString&, int size);
    void limitCheck(const TSourceLoc&, int value, const char* limit, const char* feature);

    void inductiveLoopBodyCheck(TIntermNode*, int loopIndexId, TSymbolTable&);
    void constantIndexExpressionCheck(TIntermNode*);

    void setLayoutQualifier(const TSourceLoc&, TPublicType&, TString&);
    void setLayoutQualifier(const TSourceLoc&, TPublicType&, TString&, const TIntermTyped*);
    void mergeObjectLayoutQualifiers(TQualifier& dest, const TQualifier& src, bool inheritOnly);
    void layoutObjectCheck(const TSourceLoc&, const TSymbol&);
    void layoutTypeCheck(const TSourceLoc&, const TType&);
    void layoutQualifierCheck(const TSourceLoc&, const TQualifier&);
    void checkNoShaderLayouts(const TSourceLoc&, const TShaderQualifiers&);
    void fixOffset(const TSourceLoc&, TSymbol&);

    const TFunction* findFunction(const TSourceLoc& loc, const TFunction& call, bool& builtIn);
    const TFunction* findFunctionExact(const TSourceLoc& loc, const TFunction& call, bool& builtIn);
    const TFunction* findFunction120(const TSourceLoc& loc, const TFunction& call, bool& builtIn);
    const TFunction* findFunction400(const TSourceLoc& loc, const TFunction& call, bool& builtIn);
    void declareTypeDefaults(const TSourceLoc&, const TPublicType&);
    TIntermNode* declareVariable(const TSourceLoc&, TString& identifier, const TPublicType&, TArraySizes* typeArray = 0, TIntermTyped* initializer = 0);
    TIntermTyped* addConstructor(const TSourceLoc&, TIntermNode*, const TType&, TOperator);
    TIntermTyped* constructAggregate(TIntermNode*, const TType&, int, const TSourceLoc&);
    TIntermTyped* constructBuiltIn(const TType&, TOperator, TIntermTyped*, const TSourceLoc&, bool subset);
    void declareBlock(const TSourceLoc&, TTypeList& typeList, const TString* instanceName = 0, TArraySizes* arraySizes = 0);
    void blockStageIoCheck(const TSourceLoc&, const TQualifier&);
    void blockQualifierCheck(const TSourceLoc&, const TQualifier&, bool instanceName);
    void fixBlockLocations(const TSourceLoc&, TQualifier&, TTypeList&, bool memberWithLocation, bool memberWithoutLocation);
    void fixBlockXfbOffsets(TQualifier&, TTypeList&);
    void fixBlockUniformOffsets(TQualifier&, TTypeList&);
    void addQualifierToExisting(const TSourceLoc&, TQualifier, const TString& identifier);
    void addQualifierToExisting(const TSourceLoc&, TQualifier, TIdentifierList&);
    void invariantCheck(const TSourceLoc&, const TQualifier&);
    void updateStandaloneQualifierDefaults(const TSourceLoc&, const TPublicType&);
    void wrapupSwitchSubsequence(TIntermAggregate* statements, TIntermNode* branchNode);
    TIntermNode* addSwitch(const TSourceLoc&, TIntermTyped* expression, TIntermAggregate* body);

    void updateImplicitArraySize(const TSourceLoc&, TIntermNode*, int index);

protected:
    void nonInitConstCheck(const TSourceLoc&, TString& identifier, TType& type);
    void inheritGlobalDefaults(TQualifier& dst) const;
    TVariable* makeInternalVariable(const char* name, const TType&) const;
    TVariable* declareNonArray(const TSourceLoc&, TString& identifier, TType&, bool& newDeclaration);
    void declareArray(const TSourceLoc&, TString& identifier, const TType&, TSymbol*&, bool& newDeclaration);
    TIntermNode* executeInitializer(const TSourceLoc&, TIntermTyped* initializer, TVariable* variable);
    TIntermTyped* convertInitializerList(const TSourceLoc&, const TType&, TIntermTyped* initializer);
    TOperator mapTypeToConstructorOp(const TType&) const;
    void finalErrorCheck();
    void outputMessage(const TSourceLoc&, const char* szReason, const char* szToken,
                       const char* szExtraInfoFormat, TPrefixType prefix,
                       va_list args);

public:
    //
    // Generally, bison productions, the scanner, and the PP need read/write access to these; just give them direct access
    //

    // Current state of parsing
    struct TPragma contextPragma;
    int loopNestingLevel;        // 0 if outside all loops
    int structNestingLevel;      // 0 if outside blocks and structures
    int controlFlowNestingLevel; // 0 if outside all flow control
    int statementNestingLevel;   // 0 if outside all flow control or compound statements
    TList<TIntermSequence*> switchSequenceStack;  // case, node, case, case, node, ...; ensure only one node between cases;   stack of them for nesting
    TList<int> switchLevel;      // the statementNestingLevel the current switch statement is at, which must match the level of its case statements
    bool inMain;                 // if inside a function, true if the function is main
    bool postMainReturn;         // if inside a function, true if the function is main and this is after a return statement
    const TType* currentFunctionType;  // the return type of the function that's currently being parsed
    bool functionReturnsValue;   // true if a non-void function has a return
    const TString* blockName;
    TQualifier currentBlockQualifier;
    TPrecisionQualifier defaultPrecision[EbtNumTypes];
    TBuiltInResource resources;
    TLimits& limits;

protected:
    TParseContext(TParseContext&);
    TParseContext& operator=(TParseContext&);

    const bool parsingBuiltins;        // true if parsing built-in symbols/functions
    static const int maxSamplerIndex = EsdNumDims * (EbtNumTypes * (2 * 2 * 2 * 2 * 2)); // see computeSamplerTypeIndex()
    TPrecisionQualifier defaultSamplerPrecision[maxSamplerIndex];
    bool afterEOF;
    TQualifier globalBufferDefaults;
    TQualifier globalUniformDefaults;
    TQualifier globalInputDefaults;
    TQualifier globalOutputDefaults;
    int* atomicUintOffsets;       // to become an array of the right size to hold an offset per binding point
    TString currentCaller;        // name of last function body entered (not valid when at global scope)
    TIdSetType inductiveLoopIds;
    bool anyIndexLimits;
    TVector<TIntermTyped*> needsIndexLimitationChecking;

    //
    // Geometry shader input arrays:
    //  - array sizing is based on input primitive and/or explicit size
    //
    // Tessellation control output arrays:
    //  - array sizing is based on output layout(vertices=...) and/or explicit size
    //
    // Both:
    //  - array sizing is retroactive
    //  - built-in block redeclarations interact with this
    //
    // Design:
    //  - use a per-context "resize-list", a list of symbols whose array sizes
    //    can be fixed
    //
    //  - the resize-list starts empty at beginning of user-shader compilation, it does
    //    not have built-ins in it
    //
    //  - on built-in array use: copyUp() symbol and add it to the resize-list
    //
    //  - on user array declaration: add it to the resize-list
    //
    //  - on block redeclaration: copyUp() symbol and add it to the resize-list
    //     * note, that appropriately gives an error if redeclaring a block that
    //       was already used and hence already copied-up
    //
    //  - on seeing a layout declaration that sizes the array, fix everything in the 
    //    resize-list, giving errors for mismatch
    //
    //  - on seeing an array size declaration, give errors on mismatch between it and previous
    //    array-sizing declarations
    //
    TVector<TSymbol*> ioArraySymbolResizeList;
};

} // end namespace glslang

#endif // _PARSER_HELPER_INCLUDED_
