#include "clay.hpp"

static TypePtr objectType(ObjectPtr x);
static ObjectPtr unwrapStaticType(TypePtr t);

static bool staticToType(ObjectPtr x, TypePtr &out);
static TypePtr staticToType(MultiStaticPtr x, unsigned index);
static TypePtr valueToType(MultiPValuePtr x, unsigned index);
static TypePtr valueToNumericType(MultiPValuePtr x, unsigned index);
static IntegerTypePtr valueToIntegerType(MultiPValuePtr x, unsigned index);
static TypePtr valueToPointerLikeType(MultiPValuePtr x, unsigned index);
static TypePtr valueToEnumerationType(MultiPValuePtr x, unsigned index);

static bool staticToTypeTuple(ObjectPtr x, vector<TypePtr> &out);
static void staticToTypeTuple(MultiStaticPtr x, unsigned index,
                             vector<TypePtr> &out);
static bool staticToInt(ObjectPtr x, int &out);
static int staticToInt(MultiStaticPtr x, unsigned index);
static bool staticToSizeT(ObjectPtr x, size_t &out);

static TypePtr numericTypeOfValue(MultiPValuePtr x, unsigned index);
static IntegerTypePtr integerTypeOfValue(MultiPValuePtr x, unsigned index);
static PointerTypePtr pointerTypeOfValue(MultiPValuePtr x, unsigned index);
static ArrayTypePtr arrayTypeOfValue(MultiPValuePtr x, unsigned index);
static TupleTypePtr tupleTypeOfValue(MultiPValuePtr x, unsigned index);
static RecordTypePtr recordTypeOfValue(MultiPValuePtr x, unsigned index);

static PValuePtr staticPValue(ObjectPtr x);
static PValuePtr kernelPValue(const string &name);



//
// utility procs
//

static TypePtr objectType(ObjectPtr x)
{
    switch (x->objKind) {

    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        return y->type;
    }

    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case RECORD :
    case VARIANT :
    case MODULE_HOLDER :
    case IDENTIFIER :
        return staticType(x);

    default :
        error("untypeable object");
        return NULL;

    }
}

static ObjectPtr unwrapStaticType(TypePtr t) {
    if (t->typeKind != STATIC_TYPE)
        return NULL;
    StaticType *st = (StaticType *)t.ptr();
    return st->obj;
}

static bool staticToType(ObjectPtr x, TypePtr &out)
{
    if (x->objKind != TYPE)
        return false;
    out = (Type *)x.ptr();
    return true;
}

static TypePtr staticToType(MultiStaticPtr x, unsigned index)
{
    TypePtr out;
    if (!staticToType(x->values[index], out))
        argumentError(index, "expecting a type");
    return out;
}

static TypePtr valueToType(MultiPValuePtr x, unsigned index)
{
    ObjectPtr obj = unwrapStaticType(x->values[index]->type);
    if (!obj)
        argumentError(index, "expecting a type");
    TypePtr t;
    if (!staticToType(obj, t))
        argumentError(index, "expecting a type");
    return t;
}

static TypePtr valueToNumericType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    switch (t->typeKind) {
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        return t;
    default :
        argumentError(index, "expecting a numeric type");
        return NULL;
    }
}

static IntegerTypePtr valueToIntegerType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    if (t->typeKind != INTEGER_TYPE)
        argumentError(index, "expecting an integer type");
    return (IntegerType *)t.ptr();
}

static TypePtr valueToPointerLikeType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    switch (t->typeKind) {
    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
        return t;
    default :
        argumentError(index, "expecting a pointer type");
        return NULL;
    }
}

static TypePtr valueToEnumerationType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    if (t->typeKind != ENUM_TYPE)
        argumentError(index, "expecting an enumeration type");
    return t;
}

static bool staticToTypeTuple(ObjectPtr x, vector<TypePtr> &out)
{
    TypePtr t;
    if (staticToType(x, t)) {
        out.push_back(t);
        return true;
    }
    if (x->objKind != VALUE_HOLDER)
        return false;
    ValueHolderPtr y = (ValueHolder *)x.ptr();
    assert(y->type->typeKind != STATIC_TYPE);
    if (y->type->typeKind != TUPLE_TYPE)
        return false;
    TupleTypePtr z = (TupleType *)y->type.ptr();
    for (unsigned i = 0; i < z->elementTypes.size(); ++i) {
        ObjectPtr obj = unwrapStaticType(z->elementTypes[i]);
        if ((!obj) || (obj->objKind != TYPE))
            return false;
        out.push_back((Type *)obj.ptr());
    }
    return true;
}

static void staticToTypeTuple(MultiStaticPtr x, unsigned index,
                             vector<TypePtr> &out)
{
    if (!staticToTypeTuple(x->values[index], out))
        argumentError(index, "expecting zero-or-more types");
}

static bool staticToInt(ObjectPtr x, int &out)
{
    if (x->objKind != VALUE_HOLDER)
        return false;
    ValueHolderPtr vh = (ValueHolder *)x.ptr();
    if (vh->type != cIntType)
        return false;
    out = *((int *)vh->buf);
    return true;
}

static int staticToInt(MultiStaticPtr x, unsigned index)
{
    int out = -1;
    if (!staticToInt(x->values[index], out))
        argumentError(index, "expecting Int value");
    return out;
}

static bool staticToSizeT(ObjectPtr x, size_t &out)
{
    if (x->objKind != VALUE_HOLDER)
        return false;
    ValueHolderPtr vh = (ValueHolder *)x.ptr();
    if (vh->type != cSizeTType)
        return false;
    out = *((size_t *)vh->buf);
    return true;
}

static TypePtr numericTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    switch (t->typeKind) {
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        return t;
    default :
        argumentError(index, "expecting a numeric value");
        return NULL;
    }
}

static IntegerTypePtr integerTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != INTEGER_TYPE)
        argumentError(index, "expecting an integer value");
    return (IntegerType *)t.ptr();
}

static PointerTypePtr pointerTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != POINTER_TYPE)
        argumentError(index, "expecting a pointer value");
    return (PointerType *)t.ptr();
}

static ArrayTypePtr arrayTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != ARRAY_TYPE)
        argumentError(index, "expecting an array");
    return (ArrayType *)t.ptr();
}

static TupleTypePtr tupleTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != TUPLE_TYPE)
        argumentError(index, "expecting a tuple");
    return (TupleType *)t.ptr();
}

static RecordTypePtr recordTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != RECORD_TYPE)
        argumentError(index, "expecting a record");
    return (RecordType *)t.ptr();
}

static VariantTypePtr variantTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != VARIANT_TYPE)
        argumentError(index, "expecting a variant");
    return (VariantType *)t.ptr();
}

static PValuePtr staticPValue(ObjectPtr x)
{
    TypePtr t = staticType(x);
    return new PValue(t, true);
}

static PValuePtr kernelPValue(const string &name)
{
    return staticPValue(kernelName(name));
}



//
// analyzeMulti
//

MultiPValuePtr analyzeMulti(const vector<ExprPtr> &exprs, EnvPtr env)
{
    MultiPValuePtr out = new MultiPValue();
    for (unsigned i = 0; i < exprs.size(); ++i) {
        ExprPtr x = exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr z = analyzeExpr(y->expr, env);
            if (!z)
                return NULL;
            out->add(z);
        }
        else {
            PValuePtr y = analyzeOne(x, env);
            if (!y)
                return NULL;
            out->add(y);
        }
    }
    return out;
}



//
// analyzeOne
//

PValuePtr analyzeOne(ExprPtr expr, EnvPtr env)
{
    MultiPValuePtr x = analyzeExpr(expr, env);
    if (!x)
        return NULL;
    LocationContext loc(expr->location);
    ensureArity(x, 1);
    return x->values[0];
}



//
// analyzeMultiArgs, analyzeOneArg, analyzeArgExpr
//

MultiPValuePtr analyzeMultiArgs(const vector<ExprPtr> &exprs,
                                EnvPtr env,
                                vector<bool> &dispatchFlags)
{
    MultiPValuePtr out = new MultiPValue();
    for (unsigned i = 0; i < exprs.size(); ++i) {
        ExprPtr x = exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr z = analyzeArgExpr(y->expr, env, dispatchFlags);
            if (!z)
                return NULL;
            out->add(z);
        }
        else {
            bool dispatchFlag = false;
            PValuePtr y = analyzeOneArg(x, env, dispatchFlag);
            if (!y)
                return NULL;
            out->add(y);
            dispatchFlags.push_back(dispatchFlag);
        }
    }
    assert(dispatchFlags.size() == out->size());
    return out;
}

PValuePtr analyzeOneArg(ExprPtr x, EnvPtr env, bool &dispatchFlag)
{
    vector<bool> dispatchFlags;
    MultiPValuePtr mpv = analyzeArgExpr(x, env, dispatchFlags);
    if (!mpv)
        return NULL;
    LocationContext loc(x->location);
    ensureArity(mpv, 1);
    assert(dispatchFlags.size() == 1);
    dispatchFlag = dispatchFlags[0];
    return mpv->values[0];
}

MultiPValuePtr analyzeArgExpr(ExprPtr x,
                              EnvPtr env,
                              vector<bool> &dispatchFlags)
{
    if (x->exprKind == DISPATCH_EXPR) {
        DispatchExpr *y = (DispatchExpr *)x.ptr();
        MultiPValuePtr mpv = analyzeExpr(y->expr, env);
        if (!mpv)
            return NULL;
        dispatchFlags.insert(dispatchFlags.end(), mpv->size(), true);
        return mpv;
    }
    MultiPValuePtr mpv = analyzeExpr(x, env);
    if (!mpv)
        return NULL;
    dispatchFlags.insert(dispatchFlags.end(), mpv->size(), false);
    return mpv;
}



//
// analyzeExpr
//

MultiPValuePtr analyzeExpr(ExprPtr expr, EnvPtr env)
{
    LocationContext loc(expr->location);

    switch (expr->exprKind) {

    case BOOL_LITERAL : {
        return new MultiPValue(new PValue(boolType, true));
    }

    case INT_LITERAL : {
        IntLiteral *x = (IntLiteral *)expr.ptr();
        ValueHolderPtr v = parseIntLiteral(x);
        return new MultiPValue(new PValue(v->type, true));
    }
        
    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.ptr();
        ValueHolderPtr v = parseFloatLiteral(x);
        return new MultiPValue(new PValue(v->type, true));
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarCharLiteral(x->value);
        return analyzeExpr(x->desugared, env);
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        TypePtr ptrInt8Type = pointerType(int8Type);
        PValuePtr pv = new PValue(arrayType(int8Type, x->value.size()), true);
        MultiPValuePtr args = new MultiPValue();
        args->add(new PValue(ptrInt8Type, true));
        args->add(new PValue(ptrInt8Type, true));
        return analyzeCallValue(kernelPValue("StringConstant"), args);
    }

    case IDENTIFIER_LITERAL : {
        IdentifierLiteral *x = (IdentifierLiteral *)expr.ptr();
        return new MultiPValue(staticPValue(x->value.ptr()));
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = safeLookupEnv(env, x->name);
        if (y->objKind == EXPRESSION) {
            ExprPtr z = (Expr *)y.ptr();
            return analyzeExpr(z, env);
        }
        else if (y->objKind == MULTI_EXPR) {
            MultiExprPtr z = (MultiExpr *)y.ptr();
            return analyzeMulti(z->values, env);
        }
        return analyzeStaticObject(y);
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        if ((x->args.size() == 1) &&
            (x->args[0]->exprKind != UNPACK))
        {
            return analyzeExpr(x->args[0], env);
        }
        return analyzeCallExpr(kernelNameRef("Tuple"), x->args, env);
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        return analyzeCallExpr(kernelNameRef("Array"), x->args, env);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        return analyzeIndexingExpr(x->expr, x->args, env);
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        return analyzeCallExpr(x->expr, x->args, env);
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        return analyzeFieldRefExpr(x->expr, x->name, env);
    }

    case STATIC_INDEXING : {
        StaticIndexing *x = (StaticIndexing *)expr.ptr();
        vector<ExprPtr> args;
        args.push_back(x->expr);
        ValueHolderPtr vh = sizeTToValueHolder(x->index);
        args.push_back(new StaticExpr(new ObjectExpr(vh.ptr())));
        return analyzeCallExpr(kernelNameRef("staticIndex"), args, env);
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
        if (x->op == ADDRESS_OF) {
            PValuePtr pv = analyzeOne(x->expr, env);
            if (!pv)
                return NULL;
            if (pv->isTemp)
                error("can't take address of a temporary");
        }
        if (!x->desugared)
            x->desugared = desugarUnaryOp(x);
        return analyzeExpr(x->desugared, env);
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarBinaryOp(x);
        return analyzeExpr(x->desugared, env);
    }

    case AND : {
        PValuePtr pv = new PValue(boolType, true);
        return new MultiPValue(pv);
    }

    case OR : {
        PValuePtr pv = new PValue(boolType, true);
        return new MultiPValue(pv);
    }

    case LAMBDA : {
        Lambda *x = (Lambda *)expr.ptr();
        if (!x->initialized)
            initializeLambda(x, env);
        return analyzeExpr(x->converted, env);
    }

    case UNPACK : {
        error("incorrect usage of unpack operator");
        return NULL;
    }

    case NEW : {
        New *x = (New *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarNew(x);
        return analyzeExpr(x->desugared, env);
    }

    case STATIC_EXPR : {
        StaticExpr *x = (StaticExpr *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStaticExpr(x);
        return analyzeExpr(x->desugared, env);
    }

    case DISPATCH_EXPR : {
        error("incorrect usage of dispatch operator");
        return NULL;
    }

    case FOREIGN_EXPR : {
        ForeignExpr *x = (ForeignExpr *)expr.ptr();
        return analyzeExpr(x->expr, x->getEnv());
    }

    case OBJECT_EXPR : {
        ObjectExpr *x = (ObjectExpr *)expr.ptr();
        return analyzeStaticObject(x->obj);
    }

    default :
        assert(false);
        return NULL;

    }
}



//
// analyzeStaticObject
//

MultiPValuePtr analyzeStaticObject(ObjectPtr x)
{
    switch (x->objKind) {

    case ENUM_MEMBER : {
        EnumMember *y = (EnumMember *)x.ptr();
        PValuePtr pv = new PValue(y->type, true);
        return new MultiPValue(pv);
    }

    case GLOBAL_VARIABLE : {
        GlobalVariable *y = (GlobalVariable *)x.ptr();
        PValuePtr pv = analyzeGlobalVariable(y);
        return new MultiPValue(pv);
    }

    case EXTERNAL_VARIABLE : {
        ExternalVariable *y = (ExternalVariable *)x.ptr();
        PValuePtr pv = analyzeExternalVariable(y);
        return new MultiPValue(pv);
    }

    case EXTERNAL_PROCEDURE : {
        ExternalProcedure *y = (ExternalProcedure *)x.ptr();
        analyzeExternalProcedure(y);
        PValuePtr pv = new PValue(y->ptrType, true);
        return new MultiPValue(pv);
    }

    case GLOBAL_ALIAS : {
        GlobalAlias *y = (GlobalAlias *)x.ptr();
        if (y->hasParams()) {
            TypePtr t = staticType(x);
            PValuePtr pv = new PValue(t, true);
            return new MultiPValue(pv);
        }
        return analyzeExpr(y->expr, y->env);
    }

    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        PValuePtr pv = new PValue(y->type, true);
        return new MultiPValue(pv);
    }

    case MULTI_STATIC : {
        MultiStatic *y = (MultiStatic *)x.ptr();
        MultiPValuePtr mpv = new MultiPValue();
        for (unsigned i = 0; i < y->size(); ++i) {
            TypePtr t = objectType(y->values[i]);
            mpv->values.push_back(new PValue(t, true));
        }
        return mpv;
    }

    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case RECORD :
    case VARIANT :
    case MODULE_HOLDER :
    case IDENTIFIER : {
        TypePtr t = staticType(x);
        PValuePtr pv = new PValue(t, true);
        return new MultiPValue(pv);
    }

    case EVALUE : {
        EValue *y = (EValue *)x.ptr();
        PValuePtr pv = new PValue(y->type, y->forwardedRValue);
        return new MultiPValue(pv);
    }

    case MULTI_EVALUE : {
        MultiEValue *y = (MultiEValue *)x.ptr();
        MultiPValuePtr z = new MultiPValue();
        for (unsigned i = 0; i < y->values.size(); ++i) {
            EValue *ev = y->values[i].ptr();
            z->values.push_back(new PValue(ev->type, ev->forwardedRValue));
        }
        return z;
    }

    case CVALUE : {
        CValue *y = (CValue *)x.ptr();
        PValuePtr pv = new PValue(y->type, y->forwardedRValue);
        return new MultiPValue(pv);
    }

    case MULTI_CVALUE : {
        MultiCValue *y = (MultiCValue *)x.ptr();
        MultiPValuePtr z = new MultiPValue();
        for (unsigned i = 0; i < y->values.size(); ++i) {
            CValue *cv = y->values[i].ptr();
            z->values.push_back(new PValue(cv->type, cv->forwardedRValue));
        }
        return z;
    }

    case PVALUE : {
        PValue *y = (PValue *)x.ptr();
        return new MultiPValue(y);
    }

    case MULTI_PVALUE : {
        MultiPValue *y = (MultiPValue *)x.ptr();
        return y;
    }

    case PATTERN :
        error("pattern variable cannot be used as value");
        return NULL;

    default :
        error("invalid static object");
        return NULL;

    }
}



//
// analyzeGlobalVariable
//

PValuePtr analyzeGlobalVariable(GlobalVariablePtr x)
{
    if (!x->type) {
        if (x->analyzing)
            return NULL;
        x->analyzing = true;
        PValuePtr pv = analyzeOne(x->expr, x->env);
        x->analyzing = false;
        if (!pv)
            return NULL;
        x->type = pv->type;
    }
    return new PValue(x->type, false);
}



//
// analyzeExternalVariable
//

PValuePtr analyzeExternalVariable(ExternalVariablePtr x)
{
    if (!x->type2)
        x->type2 = evaluateType(x->type, x->env);
    return new PValue(x->type2, false);
}



//
// analyzeExternalProcedure
//

void analyzeExternalProcedure(ExternalProcedurePtr x)
{
    if (x->analyzed)
        return;
    if (!x->attributesVerified)
        verifyAttributes(x);
    vector<TypePtr> argTypes;
    for (unsigned i = 0; i < x->args.size(); ++i) {
        ExternalArgPtr y = x->args[i];
        y->type2 = evaluateType(y->type, x->env);
        argTypes.push_back(y->type2);
    }
    if (!x->returnType)
        x->returnType2 = NULL;
    else
        x->returnType2 = evaluateType(x->returnType, x->env);
    CallingConv callingConv = CC_DEFAULT;
    if (x->attrStdCall)
        callingConv = CC_STDCALL;
    else if (x->attrFastCall)
        callingConv = CC_FASTCALL;
    x->ptrType = cCodePointerType(callingConv, argTypes,
                                  x->hasVarArgs, x->returnType2);
    x->analyzed = true;
}



//
// verifyAttributes
//

void verifyAttributes(ExternalProcedurePtr x)
{
    assert(!x->attributesVerified);
    x->attributesVerified = true;
    x->attrDLLImport = false;
    x->attrDLLExport = false;
    x->attrStdCall = false;
    x->attrFastCall = false;
    x->attrAsmLabel = "";
    for (unsigned i = 0; i < x->attributes.size(); ++i) {
        ExprPtr expr = x->attributes[i];
        if (expr->exprKind == NAME_REF) {
            const string &s = ((NameRef *)expr.ptr())->name->str;
            if (s == "dllimport") {
                if (x->attrDLLExport)
                    error(expr, "dllimport specified after dllexport");
                x->attrDLLImport = true;
            }
            else if (s == "dllexport") {
                if (x->attrDLLImport)
                    error(expr, "dllexport specified after dllimport");
                x->attrDLLExport = true;
            }
            else if (s == "stdcall") {
                if (x->attrFastCall)
                    error(expr, "stdcall specified after fastcall");
                x->attrStdCall = true;
            }
            else if (s == "fastcall") {
                if (x->attrStdCall)
                    error(expr, "fastcall specified after stdcall");
                x->attrFastCall = true;
            }
            else {
                error(expr, "invalid attribute");
            }
        }
        else if (expr->exprKind == STRING_LITERAL) {
            StringLiteral *y = (StringLiteral *)expr.ptr();
            x->attrAsmLabel = y->value;
        }
        else {
            error(expr, "invalid attribute");
        }
    }
}

void verifyAttributes(ExternalVariablePtr x)
{
    assert(!x->attributesVerified);
    x->attributesVerified = true;
    x->attrDLLImport = false;
    x->attrDLLExport = false;
    for (unsigned i = 0; i < x->attributes.size(); ++i) {
        ExprPtr expr = x->attributes[i];
        if (expr->exprKind != NAME_REF)
            error(expr, "invalid attribute");
        const string &s = ((NameRef *)expr.ptr())->name->str;
        if (s == "dllimport") {
            if (x->attrDLLExport)
                error(expr, "dllimport specified after dllexport");
            x->attrDLLImport = true;
        }
        else if (s == "dllexport") {
            if (x->attrDLLImport)
                error(expr, "dllexport specified after dllimport");
            x->attrDLLExport = true;
        }
        else {
            error(expr, "invalid attribute");
        }
    }
}



//
// analyzeIndexingExpr
//

static bool isTypeConstructor(ObjectPtr x) {
    if (x->objKind == PRIM_OP) {
        PrimOpPtr y = (PrimOp *)x.ptr();
        switch (y->primOpCode) {
        case PRIM_Pointer :
        case PRIM_CodePointer :
        case PRIM_RefCodePointer :
        case PRIM_CCodePointer :
        case PRIM_StdCallCodePointer :
        case PRIM_FastCallCodePointer :
        case PRIM_Array :
        case PRIM_Vec :
        case PRIM_Tuple :
        case PRIM_Union :
        case PRIM_Static :
            return true;
        default :
            return false;
        }
    }
    else if (x->objKind == RECORD) {
        return true;
    }
    else if (x->objKind == VARIANT) {
        return true;
    }
    else {
        return false;
    }
}

MultiPValuePtr analyzeIndexingExpr(ExprPtr indexable,
                                   const vector<ExprPtr> &args,
                                   EnvPtr env)
{
    PValuePtr pv = analyzeOne(indexable, env);
    if (!pv)
        return NULL;
    ObjectPtr obj = unwrapStaticType(pv->type);
    if (obj.ptr()) {
        if (isTypeConstructor(obj)) {
            MultiStaticPtr params = evaluateMultiStatic(args, env);
            PValuePtr out = analyzeTypeConstructor(obj, params);
            return new MultiPValue(out);
        }
        if (obj->objKind == GLOBAL_ALIAS) {
            GlobalAlias *x = (GlobalAlias *)obj.ptr();
            return analyzeAliasIndexing(x, args, env);
        }
        if (obj->objKind != VALUE_HOLDER)
            error("invalid indexing operation");
    }
    vector<ExprPtr> args2;
    args2.push_back(indexable);
    args2.insert(args2.end(), args.begin(), args.end());
    return analyzeCallExpr(kernelNameRef("index"), args2, env);
}



//
// constructType
//

TypePtr constructType(ObjectPtr constructor, MultiStaticPtr args)
{
    if (constructor->objKind == PRIM_OP) {
        PrimOpPtr x = (PrimOp *)constructor.ptr();

        switch (x->primOpCode) {

        case PRIM_Pointer : {
            ensureArity(args, 1);
            TypePtr pointeeType = staticToType(args, 0);
            return pointerType(pointeeType);
        }

        case PRIM_CodePointer :
        case PRIM_RefCodePointer : {
            ensureArity(args, 2);
            vector<TypePtr> argTypes;
            staticToTypeTuple(args, 0, argTypes);
            vector<TypePtr> returnTypes;
            staticToTypeTuple(args, 1, returnTypes);
            bool byRef = (x->primOpCode == PRIM_RefCodePointer);
            vector<bool> returnIsRef(returnTypes.size(), byRef);
            return codePointerType(argTypes, returnIsRef, returnTypes);
        }

        case PRIM_CCodePointer :
        case PRIM_StdCallCodePointer :
        case PRIM_FastCallCodePointer : {
            ensureArity(args, 2);
            vector<TypePtr> argTypes;
            staticToTypeTuple(args, 0, argTypes);
            vector<TypePtr> returnTypes;
            staticToTypeTuple(args, 1, returnTypes);
            if (returnTypes.size() > 1)
                argumentError(1, "C code cannot return more than one value");
            TypePtr returnType;
            if (returnTypes.size() == 1)
                returnType = returnTypes[0];
            CallingConv cc;
            switch (x->primOpCode) {
            case PRIM_CCodePointer : cc = CC_DEFAULT; break;
            case PRIM_StdCallCodePointer : cc = CC_STDCALL; break;
            case PRIM_FastCallCodePointer : cc = CC_FASTCALL; break;
            default : assert(false);
            }
            return cCodePointerType(cc, argTypes, false, returnType);
        }

        case PRIM_Array : {
            ensureArity(args, 2);
            TypePtr t = staticToType(args, 0);
            int size = staticToInt(args, 1);
            return arrayType(t, size);
        }

        case PRIM_Vec : {
            ensureArity(args, 2);
            TypePtr t = staticToType(args, 0);
            int size = staticToInt(args, 1);
            return vecType(t, size);
        }

        case PRIM_Tuple : {
            vector<TypePtr> types;
            for (unsigned i = 0; i < args->size(); ++i)
                types.push_back(staticToType(args, i));
            return tupleType(types);
        }

        case PRIM_Union : {
            vector<TypePtr> types;
            for (unsigned i = 0; i < args->size(); ++i)
                types.push_back(staticToType(args, i));
            return unionType(types);
        }

        case PRIM_Static : {
            ensureArity(args, 1);
            return staticType(args->values[0]);
        }

        default :
            assert(false);
            return NULL;
        }
    }
    else if (constructor->objKind == RECORD) {
        RecordPtr x = (Record *)constructor.ptr();
        if (x->varParam.ptr()) {
            if (args->size() < x->params.size())
                arityError2(x->params.size(), args->size());
        }
        else {
            ensureArity(args, x->params.size());
        }
        return recordType(x, args->values);
    }
    else if (constructor->objKind == VARIANT) {
        VariantPtr x = (Variant *)constructor.ptr();
        if (x->varParam.ptr()) {
            if (args->size() < x->params.size())
                arityError2(x->params.size(), args->size());
        }
        else {
            ensureArity(args, x->params.size());
        }
        return variantType(x, args->values);
    }
    else {
        assert(false);
        return NULL;
    }
}



//
// analyzeTypeConstructor
//

PValuePtr analyzeTypeConstructor(ObjectPtr obj, MultiStaticPtr args)
{
    TypePtr t = constructType(obj, args);
    return staticPValue(t.ptr());
}



//
// analyzeAliasIndexing
//

MultiPValuePtr analyzeAliasIndexing(GlobalAliasPtr x,
                                    const vector<ExprPtr> &args,
                                    EnvPtr env)
{
    assert(x->hasParams());
    MultiStaticPtr params = evaluateMultiStatic(args, env);
    if (x->varParam.ptr()) {
        if (params->size() < x->params.size())
            arityError2(x->params.size(), params->size());
    }
    else {
        ensureArity(params, x->params.size());
    }
    EnvPtr bodyEnv = new Env(x->env);
    for (unsigned i = 0; i < x->params.size(); ++i) {
        addLocal(bodyEnv, x->params[i], params->values[i]);
    }
    if (x->varParam.ptr()) {
        MultiStaticPtr varParams = new MultiStatic();
        for (unsigned i = x->params.size(); i < params->size(); ++i)
            varParams->add(params->values[i]);
        addLocal(bodyEnv, x->varParam, varParams.ptr());
    }
    return analyzeExpr(x->expr, bodyEnv);
}



//
// analyzeFieldRefExpr
//

MultiPValuePtr analyzeFieldRefExpr(ExprPtr base,
                                   IdentifierPtr name,
                                   EnvPtr env)
{
    PValuePtr pv = analyzeOne(base, env);
    if (!pv)
        return NULL;
    ObjectPtr obj = unwrapStaticType(pv->type);
    if (obj.ptr()) {
        if (obj->objKind == MODULE_HOLDER) {
            ModuleHolderPtr y = (ModuleHolder *)obj.ptr();
            ObjectPtr z = safeLookupModuleHolder(y, name);
            return analyzeStaticObject(z);
        }
        if (obj->objKind != VALUE_HOLDER)
            error("invalid field access");
    }
    vector<ExprPtr> args;
    args.push_back(base);
    args.push_back(new ObjectExpr(name.ptr()));
    return analyzeCallExpr(kernelNameRef("fieldRef"), args, env);
}



//
// computeArgsKey, analyzeReturn
//

void computeArgsKey(MultiPValuePtr args,
                    vector<TypePtr> &argsKey,
                    vector<ValueTempness> &argsTempness)
{
    for (unsigned i = 0; i < args->size(); ++i) {
        PValuePtr pv = args->values[i];
        argsKey.push_back(pv->type);
        argsTempness.push_back(pv->isTemp ? TEMPNESS_RVALUE : TEMPNESS_LVALUE);
    }
}

MultiPValuePtr analyzeReturn(const vector<bool> &returnIsRef,
                             const vector<TypePtr> &returnTypes)
                             
{
    MultiPValuePtr x = new MultiPValue();
    for (unsigned i = 0; i < returnTypes.size(); ++i) {
        PValuePtr pv = new PValue(returnTypes[i], !returnIsRef[i]);
        x->values.push_back(pv);
    }
    return x.ptr();
}



//
// analyzeCallExpr
//

MultiPValuePtr analyzeCallExpr(ExprPtr callable,
                               const vector<ExprPtr> &args,
                               EnvPtr env)
{
    PValuePtr pv = analyzeOne(callable, env);
    if (!pv)
        return NULL;
    switch (pv->type->typeKind) {
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE : {
        MultiPValuePtr mpv = analyzeMulti(args, env);
        if (!mpv)
            return NULL;
        return analyzeCallPointer(pv, mpv);
    }
    }
    ObjectPtr obj = unwrapStaticType(pv->type);
    if (!obj) {
        vector<ExprPtr> args2;
        args2.push_back(callable);
        args2.insert(args2.end(), args.begin(), args.end());
        return analyzeCallExpr(kernelNameRef("call"), args2, env);
    }

    switch (obj->objKind) {

    case TYPE :
    case RECORD :
    case VARIANT :
    case PROCEDURE :
    case PRIM_OP : {
        if ((obj->objKind == PRIM_OP) && !isOverloadablePrimOp(obj)) {
            PrimOpPtr x = (PrimOp *)obj.ptr();
            MultiPValuePtr mpv = analyzeMulti(args, env);
            if (!mpv)
                return NULL;
            return analyzePrimOp(x, mpv);
        }
        vector<bool> dispatchFlags;
        MultiPValuePtr mpv = analyzeMultiArgs(args, env, dispatchFlags);
        if (!mpv)
            return NULL;
        vector<unsigned> dispatchIndices;
        for (unsigned i = 0; i < dispatchFlags.size(); ++i) {
            if (dispatchFlags[i])
                dispatchIndices.push_back(i);
        }
        if (dispatchIndices.size() > 0) {
            return analyzeDispatch(obj, mpv, dispatchIndices);
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(mpv, argsKey, argsTempness);
        InvokeStackContext invokeStackContext(obj, argsKey);
        InvokeEntryPtr entry =
            analyzeCallable(obj, argsKey, argsTempness);
        if (entry->inlined)
            return analyzeCallInlined(entry, args, env);
        if (!entry->analyzed)
            return NULL;
        return analyzeReturn(entry->returnIsRef, entry->returnTypes);
    }

    default :
        error("invalid call expression");
        return NULL;

    }
}



//
// analyzeDispatch
//

MultiPValuePtr analyzeDispatch(ObjectPtr obj,
                               MultiPValuePtr args,
                               const vector<unsigned> &dispatchIndices)
{
    if (dispatchIndices.empty())
        return analyzeCallValue(staticPValue(obj), args);

    vector<TypePtr> argsKey;
    vector<ValueTempness> argsTempness;
    computeArgsKey(args, argsKey, argsTempness);
    InvokeStackContext invokeStackContext(obj, argsKey);

    unsigned index = dispatchIndices[0];
    vector<unsigned> dispatchIndices2(dispatchIndices.begin() + 1,
                                      dispatchIndices.end());
    MultiPValuePtr prefix = new MultiPValue();
    for (unsigned i = 0; i < index; ++i)
        prefix->add(args->values[i]);
    MultiPValuePtr suffix = new MultiPValue();
    for (unsigned i = index+1; i < args->size(); ++i)
        suffix->add(args->values[i]);
    PValuePtr pvDispatch = args->values[index];
    if (pvDispatch->type->typeKind != VARIANT_TYPE) {
        argumentError(index, "dispatch operator can "
                      "only be used with variants");
    }
    VariantTypePtr t = (VariantType *)pvDispatch->type.ptr();
    const vector<TypePtr> &memberTypes = variantMemberTypes(t);
    if (memberTypes.empty())
        argumentError(index, "variant has no member types");
    MultiPValuePtr result;
    for (unsigned i = 0; i < memberTypes.size(); ++i) {
        MultiPValuePtr args2 = new MultiPValue();
        args2->add(prefix);
        args2->add(new PValue(memberTypes[i], pvDispatch->isTemp));
        args2->add(suffix);
        MultiPValuePtr result2 = analyzeDispatch(obj, args2, dispatchIndices2);
        if (!result2)
            return NULL;
        if (!result) {
            result = result2;
        }
        else {
            bool ok = true;
            if (result->size() != result2->size()) {
                ok = false;
            }
            else {
                for (unsigned j = 0; j < result2->size(); ++j) {
                    PValuePtr pv = result->values[j];
                    PValuePtr pv2 = result2->values[j];
                    if (pv->type != pv2->type) {
                        ok = false;
                        break;
                    }
                    if (pv->isTemp != pv2->isTemp) {
                        ok = false;
                        break;
                    }
                }
            }
            if (!ok) {
                ostringstream ostr;
                ostr << "mismatching result with variant member: ";
                ostr << memberTypes[i];
                argumentError(index, ostr.str());
            }
        }
    }
    assert(result.ptr());
    return result;
}



//
// analyzeCallValue
//

MultiPValuePtr analyzeCallValue(PValuePtr callable,
                                MultiPValuePtr args)
{
    switch (callable->type->typeKind) {
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
        return analyzeCallPointer(callable, args);
    }
    ObjectPtr obj = unwrapStaticType(callable->type);
    if (!obj) {
        MultiPValuePtr args2 = new MultiPValue(callable);
        args2->add(args);
        return analyzeCallValue(kernelPValue("call"), args2);
    }

    switch (obj->objKind) {

    case TYPE :
    case RECORD :
    case VARIANT :
    case PROCEDURE :
    case PRIM_OP : {
        if ((obj->objKind == PRIM_OP) && !isOverloadablePrimOp(obj)) {
            PrimOpPtr x = (PrimOp *)obj.ptr();
            return analyzePrimOp(x, args);
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(args, argsKey, argsTempness);
        InvokeStackContext invokeStackContext(obj, argsKey);
        InvokeEntryPtr entry =
            analyzeCallable(obj, argsKey, argsTempness);
        if (entry->inlined)
            error("call to inlined code is not allowed in this context");
        if (!entry->analyzed)
            return NULL;
        return analyzeReturn(entry->returnIsRef, entry->returnTypes);
    }

    default :
        error("invalid call expression");
        return NULL;

    }
}



//
// analyzeCallPointer
//

MultiPValuePtr analyzeCallPointer(PValuePtr x,
                                  MultiPValuePtr args)
{
    switch (x->type->typeKind) {

    case CODE_POINTER_TYPE : {
        CodePointerType *y = (CodePointerType *)x->type.ptr();
        return analyzeReturn(y->returnIsRef, y->returnTypes);
    }

    case CCODE_POINTER_TYPE : {
        CCodePointerType *y = (CCodePointerType *)x->type.ptr();
        if (!y->returnType)
            return new MultiPValue();
        return new MultiPValue(new PValue(y->returnType, true));
    }

    default :
        assert(false);
        return NULL;

    }
}



//
// analyzeIsDefined, analyzeCallable
//

bool analyzeIsDefined(ObjectPtr x,
                      const vector<TypePtr> &argsKey,
                      const vector<ValueTempness> &argsTempness)
{
    InvokeEntryPtr entry = lookupInvokeEntry(x, argsKey, argsTempness);

    if (!entry || !entry->code->hasBody())
        return false;
    return true;
}

InvokeEntryPtr analyzeCallable(ObjectPtr x,
                               const vector<TypePtr> &argsKey,
                               const vector<ValueTempness> &argsTempness)
{
    InvokeEntryPtr entry = lookupInvokeEntry(x, argsKey, argsTempness);

    if (!entry || !entry->code->hasBody())
        error("no matching operation");
    if (entry->analyzed || entry->analyzing)
        return entry;
    if (entry->inlined)
        return entry;

    entry->analyzing = true;
    analyzeCodeBody(entry);
    entry->analyzing = false;

    return entry;
}



//
// analyzeCallInlined
//

MultiPValuePtr analyzeCallInlined(InvokeEntryPtr entry,
                                  const vector<ExprPtr> &args,
                                  EnvPtr env)
{
    assert(entry->inlined);

    CodePtr code = entry->code;
    assert(code->body.ptr());

    if (!code->returnSpecs.empty()) {
        vector<bool> returnIsRef;
        vector<TypePtr> returnTypes;
        evaluateReturnSpecs(code->returnSpecs, entry->env,
                            returnIsRef, returnTypes);
        return analyzeReturn(returnIsRef, returnTypes);
    }

    if (entry->varArgName.ptr())
        assert(args.size() >= entry->fixedArgNames.size());
    else
        assert(args.size() == entry->fixedArgNames.size());

    EnvPtr bodyEnv = new Env(entry->env);

    for (unsigned i = 0; i < entry->fixedArgNames.size(); ++i) {
        ExprPtr expr = foreignExpr(env, args[i]);
        addLocal(bodyEnv, entry->fixedArgNames[i], expr.ptr());
    }

    if (entry->varArgName.ptr()) {
        MultiExprPtr varArgs = new MultiExpr();
        for (unsigned i = entry->fixedArgNames.size(); i < args.size(); ++i) {
            ExprPtr expr = foreignExpr(env, args[i]);
            varArgs->add(expr);
        }
        addLocal(bodyEnv, entry->varArgName, varArgs.ptr());
    }

    AnalysisContextPtr ctx = new AnalysisContext();
    bool ok = analyzeStatement(code->body, bodyEnv, ctx);
    if (!ok && !ctx->returnInitialized)
        return NULL;
    if (ctx->returnInitialized)
        return analyzeReturn(ctx->returnIsRef, ctx->returnTypes);
    else
        return new MultiPValue();
}



//
// analyzeCodeBody
//

void analyzeCodeBody(InvokeEntryPtr entry)
{
    assert(!entry->analyzed);

    CodePtr code = entry->code;
    assert(code->hasBody());

    if (code->isInlineLLVM() || !code->returnSpecs.empty()) {
        evaluateReturnSpecs(code->returnSpecs, entry->env,
                            entry->returnIsRef, entry->returnTypes);
        entry->analyzed = true;
        return;
    }

    EnvPtr bodyEnv = new Env(entry->env);

    for (unsigned i = 0; i < entry->fixedArgNames.size(); ++i) {
        bool flag = entry->forwardedRValueFlags[i];
        PValuePtr parg = new PValue(entry->fixedArgTypes[i], flag);
        addLocal(bodyEnv, entry->fixedArgNames[i], parg.ptr());
    }

    if (entry->varArgName.ptr()) {
        unsigned nFixed = entry->fixedArgNames.size();
        MultiPValuePtr varArgs = new MultiPValue();
        for (unsigned i = 0; i < entry->varArgTypes.size(); ++i) {
            bool flag = entry->forwardedRValueFlags[i + nFixed];
            PValuePtr parg = new PValue(entry->varArgTypes[i], flag);
            varArgs->values.push_back(parg);
        }
        addLocal(bodyEnv, entry->varArgName, varArgs.ptr());
    }

    AnalysisContextPtr ctx = new AnalysisContext();
    bool ok = analyzeStatement(code->body, bodyEnv, ctx);
    if (!ok && !ctx->returnInitialized)
        return;
    if (ctx->returnInitialized) {
        entry->returnIsRef = ctx->returnIsRef;
        entry->returnTypes = ctx->returnTypes;
    }
    entry->analyzed = true;
}



//
// analyzeStatement
//

bool analyzeStatement(StatementPtr stmt, EnvPtr env, AnalysisContextPtr ctx)
{
    LocationContext loc(stmt->location);

    switch (stmt->stmtKind) {

    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        for (unsigned i = 0; i < x->statements.size(); ++i) {
            StatementPtr y = x->statements[i];
            if (y->stmtKind == BINDING) {
                env = analyzeBinding((Binding *)y.ptr(), env);
                if (!env)
                    return false;
            }
            else if (!analyzeStatement(y, env, ctx)) {
                return false;
            }
        }
        return true;
    }

    case LABEL :
    case BINDING :
    case ASSIGNMENT :
    case INIT_ASSIGNMENT :
    case UPDATE_ASSIGNMENT :
    case GOTO :
        return true;

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        MultiPValuePtr mpv = analyzeMulti(x->exprs, env);
        if (!mpv) 
            return NULL;
        if (ctx->returnInitialized) {
            ensureArity(mpv, ctx->returnTypes.size());
            for (unsigned i = 0; i < mpv->size(); ++i) {
                PValuePtr pv = mpv->values[i];
                bool byRef = returnKindToByRef(x->returnKind, pv);
                if (ctx->returnTypes[i] != pv->type)
                    argumentError(i, "type mismatch");
                if (byRef != ctx->returnIsRef[i])
                    argumentError(i, "mismatching by-ref and "
                                  "by-value returns");
                if (byRef && pv->isTemp)
                    argumentError(i, "cannot return a temporary by reference");
            }
        }
        else {
            ctx->returnIsRef.clear();
            ctx->returnTypes.clear();
            for (unsigned i = 0; i < mpv->size(); ++i) {
                PValuePtr pv = mpv->values[i];
                bool byRef = returnKindToByRef(x->returnKind, pv);
                ctx->returnIsRef.push_back(byRef);
                if (byRef && pv->isTemp)
                    argumentError(i, "cannot return a temporary by reference");
                ctx->returnTypes.push_back(pv->type);
            }
            ctx->returnInitialized = true;
        }
        return true;
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        bool thenResult = analyzeStatement(x->thenPart, env, ctx);
        bool elseResult = true;
        if (x->elsePart.ptr())
            elseResult = analyzeStatement(x->elsePart, env, ctx);
        return thenResult || elseResult;
    }

    case EXPR_STATEMENT :
        return true;

    case WHILE : {
        While *x = (While *)stmt.ptr();
        analyzeStatement(x->body, env, ctx);
        return true;
    }

    case BREAK :
    case CONTINUE :
        return true;

    case FOR : {
        For *x = (For *)stmt.ptr();
        if (!x->desugared)
            x->desugared = desugarForStatement(x);
        return analyzeStatement(x->desugared, env, ctx);
    }

    case FOREIGN_STATEMENT : {
        ForeignStatement *x = (ForeignStatement *)stmt.ptr();
        return analyzeStatement(x->statement, x->getEnv(), ctx);
    }

    case TRY : {
        Try *x = (Try *)stmt.ptr();
        if (!x->desugaredCatchBlock)
            x->desugaredCatchBlock = desugarCatchBlocks(x->catchBlocks);
        bool result1 = analyzeStatement(x->tryBlock, env, ctx);
        bool result2 = analyzeStatement(x->desugaredCatchBlock, env, ctx);
        return result1 || result2;
    }

    case THROW :
        return true;

    default :
        assert(false);
        return false;
    }
}

EnvPtr analyzeBinding(BindingPtr x, EnvPtr env)
{
    LocationContext loc(x->location);

    switch (x->bindingKind) {

    case VAR :
    case REF : {
        MultiPValuePtr right = analyzeMulti(x->exprs, env);
        if (!right)
            return NULL;
        if (right->size() != x->names.size())
            arityError(x->names.size(), right->size());
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < right->size(); ++i) {
            PValuePtr pv = right->values[i];
            addLocal(env2, x->names[i], new PValue(pv->type, false));
        }
        return env2;
    }

    case ALIAS : {
        ensureArity(x->names, 1);
        ensureArity(x->exprs, 1);
        EnvPtr env2 = new Env(env);
        ExprPtr y = foreignExpr(env, x->exprs[0]);
        addLocal(env2, x->names[0], y.ptr());
        return env2;
    }

    default :
        assert(false);
        return NULL;

    }
}

bool returnKindToByRef(ReturnKind returnKind, PValuePtr pv)
{
    switch (returnKind) {
    case RETURN_VALUE : return false;
    case RETURN_REF : return true;
    case RETURN_FORWARD : return !pv->isTemp;
    default : assert(false); return false;
    }
}



//
// analyzePrimOp
//

MultiPValuePtr analyzePrimOp(PrimOpPtr x, MultiPValuePtr args)
{
    switch (x->primOpCode) {

    case PRIM_TypeP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_TypeSize :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_CallDefinedP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_primitiveCopy :
        return new MultiPValue();

    case PRIM_boolNot :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_numericEqualsP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_numericLesserP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_numericAdd :
    case PRIM_numericSubtract :
    case PRIM_numericMultiply :
    case PRIM_numericDivide : {
        ensureArity(args, 2);
        TypePtr t = numericTypeOfValue(args, 0);
        return new MultiPValue(new PValue(t, true));
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t = numericTypeOfValue(args, 0);
        return new MultiPValue(new PValue(t, true));
    }

    case PRIM_integerRemainder :
    case PRIM_integerShiftLeft :
    case PRIM_integerShiftRight :
    case PRIM_integerBitwiseAnd :
    case PRIM_integerBitwiseOr :
    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        IntegerTypePtr t = integerTypeOfValue(args, 0);
        return new MultiPValue(new PValue(t.ptr(), true));
    }

    case PRIM_integerBitwiseNot : {
        ensureArity(args, 1);
        IntegerTypePtr t = integerTypeOfValue(args, 0);
        return new MultiPValue(new PValue(t.ptr(), true));
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr t = valueToNumericType(args, 0);
        return new MultiPValue(new PValue(t, true));
    }

    case PRIM_Pointer :
        error("Pointer type constructor cannot be called");

    case PRIM_addressOf : {
        ensureArity(args, 1);
        TypePtr t = pointerType(args->values[0]->type);
        return new MultiPValue(new PValue(t, true));
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr pt = pointerTypeOfValue(args, 0);
        return new MultiPValue(new PValue(pt->pointeeType, false));
    }

    case PRIM_pointerEqualsP :
    case PRIM_pointerLesserP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr pt = pointerTypeOfValue(args, 0);
        return new MultiPValue(new PValue(pt.ptr(), true));
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        IntegerTypePtr t = valueToIntegerType(args, 0);
        return new MultiPValue(new PValue(t.ptr(), true));
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args, 0);
        return new MultiPValue(new PValue(pointerType(t), true));
    }

    case PRIM_CodePointerP : {
        return new MultiPValue(new PValue(boolType, true));
    }

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_RefCodePointer :
        error("RefCodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = unwrapStaticType(args->values[0]->type);
        if (!callable)
            argumentError(0, "static callable expected");
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case VARIANT :
        case PROCEDURE :
            break;
        case PRIM_OP :
            if (!isOverloadablePrimOp(callable))
                argumentError(0, "invalid callable");
            break;
        default :
            argumentError(0, "invalid callable");
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        for (unsigned i = 1; i < args->size(); ++i) {
            TypePtr t = valueToType(args, i);
            argsKey.push_back(t);
            argsTempness.push_back(TEMPNESS_LVALUE);
        }

        InvokeStackContext invokeStackContext(callable, argsKey);

        InvokeEntryPtr entry =
            analyzeCallable(callable, argsKey, argsTempness);
        if (entry->inlined)
            argumentError(0, "cannot create pointer to inlined code");
        if (!entry->analyzed)
            return NULL;
        TypePtr cpType = codePointerType(argsKey,
                                         entry->returnIsRef,
                                         entry->returnTypes);
        return new MultiPValue(new PValue(cpType, true));
    }

    case PRIM_CCodePointerP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_CCodePointer :
        error("CCodePointer type constructor cannot be called");

    case PRIM_StdCallCodePointer :
        error("StdCallCodePointer type constructor cannot be called");

    case PRIM_FastCallCodePointer :
        error("FastCallCodePointer type constructor cannot be called");

    case PRIM_makeCCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = unwrapStaticType(args->values[0]->type);
        if (!callable)
            argumentError(0, "static callable expected");
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case VARIANT :
        case PROCEDURE :
            break;
        case PRIM_OP :
            if (!isOverloadablePrimOp(callable))
                argumentError(0, "invalid callable");
            break;
        default :
            argumentError(0, "invalid callable");
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        for (unsigned i = 1; i < args->size(); ++i) {
            TypePtr t = valueToType(args, i);
            argsKey.push_back(t);
            argsTempness.push_back(TEMPNESS_LVALUE);
        }

        InvokeStackContext invokeStackContext(callable, argsKey);

        InvokeEntryPtr entry = analyzeCallable(callable, argsKey, argsTempness);
        if (entry->inlined)
            argumentError(0, "cannot create pointer to inlined code");
        if (!entry->analyzed)
            return NULL;
        TypePtr returnType;
        if (entry->returnTypes.empty()) {
            returnType = NULL;
        }
        else if (entry->returnTypes.size() == 1) {
            if (entry->returnIsRef[0]) {
                argumentError(0, "cannot create c-code pointer to "
                              " return-by-reference code");
            }
            returnType = entry->returnTypes[0];
        }
        else {
            argumentError(0, "cannot create c-code pointer to "
                          "multi-return code");
        }
        TypePtr ccpType = cCodePointerType(CC_DEFAULT,
                                           argsKey,
                                           false,
                                           returnType);
        return new MultiPValue(new PValue(ccpType, true));
    }

    case PRIM_pointerCast : {
        ensureArity(args, 2);
        TypePtr t = valueToPointerLikeType(args, 0);
        return new MultiPValue(new PValue(t, true));
    }

    case PRIM_Array :
        error("Array type constructor cannot be called");

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ArrayTypePtr t = arrayTypeOfValue(args, 0);
        return new MultiPValue(new PValue(t->elementType, false));
    }

    case PRIM_Vec :
        error("Vec type constructor cannot be called");

    case PRIM_TupleP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_Tuple :
        error("Tuple type constructor cannot be called");

    case PRIM_TupleElementCount :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_TupleElementOffset :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        TupleTypePtr t = tupleTypeOfValue(args, 0);
        ObjectPtr obj = unwrapStaticType(args->values[1]->type);
        size_t i = 0;
        if (!obj || !staticToSizeT(obj, i))
            argumentError(1, "expecting SizeT value");
        if (i >= t->elementTypes.size())
            argumentError(1, "tuple index out of range");
        return new MultiPValue(new PValue(t->elementTypes[i], false));
    }

    case PRIM_UnionP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_Union :
        error("Union type constructor cannot be called");

    case PRIM_UnionMemberCount :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_RecordP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_RecordFieldCount :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_RecordFieldOffset :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_RecordFieldIndex :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        RecordTypePtr t = recordTypeOfValue(args, 0);
        ObjectPtr obj = unwrapStaticType(args->values[1]->type);
        size_t i = 0;
        if (!obj || !staticToSizeT(obj, i))
            argumentError(1, "expecting SizeT value");
        const vector<TypePtr> &fieldTypes = recordFieldTypes(t);
        if (i >= fieldTypes.size())
            argumentError(1, "field index out of range");
        return new MultiPValue(new PValue(fieldTypes[i], false));
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        RecordTypePtr t = recordTypeOfValue(args, 0);
        ObjectPtr obj = unwrapStaticType(args->values[1]->type);
        if (!obj || (obj->objKind != IDENTIFIER))
            argumentError(1, "expecting field name identifier");
        IdentifierPtr fname = (Identifier *)obj.ptr();
        const map<string, size_t> &fieldIndexMap = recordFieldIndexMap(t);
        map<string, size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            argumentError(1, "field not in record");
        const vector<TypePtr> &fieldTypes = recordFieldTypes(t);
        return new MultiPValue(new PValue(fieldTypes[fi->second], false));
    }

    case PRIM_VariantP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_VariantMemberIndex :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_VariantMemberCount :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_variantRepr : {
        ensureArity(args, 1);
        VariantTypePtr t = variantTypeOfValue(args, 0);
        TypePtr reprType = variantReprType(t);
        return new MultiPValue(new PValue(reprType, false));
    }

    case PRIM_Static :
        error("Static type constructor cannot be called");

    case PRIM_StaticName : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        if (!obj)
            argumentError(0, "expecting static object");
        ostringstream sout;
        printName(sout, obj);
        ExprPtr z = new StringLiteral(sout.str());
        return analyzeExpr(z, new Env());
    }

    case PRIM_EnumP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_enumToInt :
        return new MultiPValue(new PValue(cIntType, true));

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        TypePtr t = valueToEnumerationType(args, 0);
        return new MultiPValue(new PValue(t, true));
    }

    default :
        assert(false);
        return NULL;

    }
}
