#include "V8/V8Console.h"
#include "Common/LogMacros.h"

namespace rinrin::uejs
{

    void ConsoleLogCallback(const v8::FunctionCallbackInfo<v8::Value> &args)
    {
        v8::Isolate *isolate = args.GetIsolate();
        v8::HandleScope handle_scope(isolate);

        if (args.Length() < 1 || !args[0]->IsString())
        {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8Literal(isolate, "console.log expects (string)")));
            return;
        }

        v8::String::Utf8Value s(isolate, args[0]);
        const char *cstr = *s ? *s : "";
        UEJS_LOG(LogJs, Log, TEXT("v8: %hs"), cstr);

        // TODO: 这里转到 UE：例如 UE_LOG / delegate / your bridge
        // UE_LOG(LogTemp, Log, TEXT("foo.bar: %hs"), cstr);
    }

    void FV8Console::InjectConsole(v8::Isolate *Isolate, v8::Local<v8::Context> Context)
    {
        v8::HandleScope handle_scope(Isolate);
        v8::Context::Scope context_scope(Context);

        v8::Local<v8::Object> console = v8::Object::New(Isolate);
        // Define console.log
        console->Set(Context,
                     v8::String::NewFromUtf8(Isolate, "log").ToLocalChecked(),
                     v8::Function::New(Context, ConsoleLogCallback)
                         .ToLocalChecked())
            .Check();

        // You can add more console methods like console.error, console.warn, etc. similarly.

        // Attach console to the global object
        Context->Global()
            ->Set(Context,
                  v8::String::NewFromUtf8(Isolate, "console").ToLocalChecked(),
                  console)
            .Check();
    }
}