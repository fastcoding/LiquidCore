//
// JSValue.cpp
//
// LiquidPlayer project
// https://github.com/LiquidPlayer
//
// Created by Eric Lange
//
/*
 Copyright (c) 2016-2018 Eric Lange. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 - Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 - Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <cstdlib>
#include <android/log.h>
#include <boost/make_shared.hpp>
#include "Common/JSValue.h"
#include "Common/Macros.h"

boost::shared_ptr<JSValue> JSValue::New(boost::shared_ptr<JSContext> context, Local<v8::Value> val)
{
    boost::shared_ptr<JSValue> value;

    if (val->IsObject()) {
        Local<Private> privateKey = v8::Private::ForApi(context->isolate(),
            String::NewFromUtf8(context->isolate(), "__JSValue_ptr"));
        Local<Object> obj = val->ToObject(context->Value()).ToLocalChecked();
        Local<v8::Value> identifier;
        Maybe<bool> result = obj->HasPrivate(context->Value(), privateKey);
        bool hasPrivate = false;
        if (result.IsJust() && result.FromJust()) {
            hasPrivate = obj->GetPrivate(context->Value(), privateKey).ToLocal(&identifier);
        }
        if (hasPrivate && !identifier->IsUndefined()) {
            // This object is already wrapped, let's re-use it
            return Unwrap(identifier)->shared_from_this();
        } else {
            // First time wrap.  Create it new and mark it
            value = boost::make_shared<JSValue>(context,val);
            context->retain(value);
            value->m_wrapped = true;

            obj->SetPrivate(context->Value(), privateKey, Wrap(&* value));
        }
        value->m_reference = TOOBJPTR(&*value);
    } else {
        value = boost::make_shared<JSValue>(context,val);
        value->m_reference =
                (value->m_isUndefined) ? ODDBALL_UNDEFINED :
                (value->m_isNull) ? ODDBALL_NULL :
                (val->IsBoolean() && val->IsTrue()) ? ODDBALL_TRUE :
                (val->IsBoolean()) ? ODDBALL_FALSE : -1;
        if (value->m_reference == -1 && val->IsNumber()) {
            double v = val->ToNumber(context->isolate())->Value();
            jlong *pv = (jlong *) &v;
            if (CANPRIMITIVE(*pv)) {
                value->m_reference = *pv;
            } else {
                value->m_reference = TOPTR(&*value);
            }
        } else {
            value->m_reference = TOPTR(&*value);
        }
    }

    context->Group()->Manage(value);

    return value;
}

boost::shared_ptr<JSValue> JSValue::New(boost::shared_ptr<JSContext> context, jlong thiz)
{
    if (ISPOINTER(thiz)) {
        JSValue *ptr = TOJSVALUE(thiz);
        return ptr->javaReference();
    }

    Isolate::Scope isolate_scope_(Isolate::GetCurrent());
    HandleScope handle_scope_(Isolate::GetCurrent());

    Local<v8::Value> value;

    if (ISODDBALL(thiz)) {
        switch (thiz) {
            case ODDBALL_FALSE:     value = False(Isolate::GetCurrent()); break;
            case ODDBALL_TRUE:      value = True (Isolate::GetCurrent()); break;
            case ODDBALL_UNDEFINED: value = Undefined(Isolate::GetCurrent()); break;
            case ODDBALL_NULL:      value = Null(Isolate::GetCurrent());; break;
            default: break;
        }
    } else {
        double dval = * (double *) &thiz;
        value = Number::New(Isolate::GetCurrent(), dval);
    }

    return New(context, value);
}

JSValue::JSValue(boost::shared_ptr<JSContext> context, Local<v8::Value> val)
{
    if (val->IsUndefined()) {
        m_isUndefined = true;
        m_isNull = false;
    } else if (val->IsNull()) {
        m_isUndefined = false;
        m_isNull = true;
    } else {
        m_value = Persistent<v8::Value,CopyablePersistentTraits<v8::Value>>(context->isolate(), val);
        m_isUndefined = false;
        m_isNull = false;
    }
    m_context = context;
    m_wrapped = false;
    m_isDefunct = false;
    m_count = 0;
}

JSValue::JSValue() : m_wrapped(false), m_isDefunct(false)
{
}

JSValue::~JSValue()
{
    Dispose();
}

void JSValue::Dispose()
{
    if (!m_isDefunct) {
        m_isDefunct = true;

        boost::shared_ptr<JSContext> context = m_context;
        if (context && !m_isUndefined && !m_isNull) {
            V8_ISOLATE(context->Group(), isolate)

            if (m_wrapped) {
                Local<Object> obj = Value()->ToObject(context->Value()).ToLocalChecked();
                // Clear wrapper pointer if it exists, in case this object is still held by JS
                Local<Private> privateKey = v8::Private::ForApi(isolate,
                    String::NewFromUtf8(isolate, "__JSValue_ptr"));
                obj->SetPrivate(context->Value(), privateKey,
                    Local<v8::Value>::New(isolate,Undefined(isolate)));
            }

            if (!m_value.IsEmpty()){
                Local<v8::Value> v=m_value.Get(isolate);
                if (v->IsString()) {
                    Local<v8::String> s=v->ToString();
                    __android_log_print(ANDROID_LOG_INFO, "NodeJS", "release: string length=%d",
                                        s->Utf8Length());
                }
            }
                context->Group()->removeManage(m_value.Get(isolate));
            m_value.Reset();
            context.reset();
            V8_UNLOCK()
        }

        m_isUndefined = true;
    }
}