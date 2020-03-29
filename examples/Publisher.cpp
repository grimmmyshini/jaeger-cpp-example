#include <iostream>

#include <sys/ipc.h> 

#include <sys/msg.h> 

#include <yaml-cpp/yaml.h>

#include <jaegertracing/Tracer.h>

namespace {

struct msg_buffer { 
    long msg_type; 
    char msg_text[50]; 
} message; 

//Set up the tracer 
void setUpTracer(const char* configFilePath)
{
    auto configYAML = YAML::LoadFile(configFilePath);
    auto config = jaegertracing::Config::parse(configYAML);
    auto tracer = jaegertracing::Tracer::make(
        "publisher-service", config, jaegertracing::logging::consoleLogger());
    opentracing::Tracer::InitGlobal(
        std::static_pointer_cast<opentracing::Tracer>(tracer));
}

//Publish the message 
void publishFunction(const int messageID, const std::unique_ptr<opentracing::Span>& parentSpan)
{
    auto span = opentracing::Tracer::Global()->StartSpan(
        "tracedPublishSubroutine", { opentracing::ChildOf(&parentSpan->context()) });
    msgsnd(messageID, &message, sizeof(message), 0); 
    span->SetTag("message-sent", message.msg_text);
    
}

//Set up the message publisher and message queue
void setUpPublisher(const char* textToSend)
{
    auto span = opentracing::Tracer::Global()->StartSpan("setupPublisherFunction");

    key_t key; 
    int msgid; 

    key = ftok("pub-sub", 65); 

    msgid = msgget(key, 0666 | IPC_CREAT); 
    message.msg_type = 1;
    strcpy(message.msg_text, textToSend);

    publishFunction(msgid, span);

}
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <config-yaml-path> "<<"<message-to-publish>\n";
        return 1;
    }

    else if(strlen(argv[2]) > 50) {
        std::cerr << "Message max limit is 50 characters!";
        return 1;
    }

    setUpTracer(argv[1]);

    setUpPublisher(argv[2]);

    opentracing::Tracer::Global()->Close();
    return 0;
}
