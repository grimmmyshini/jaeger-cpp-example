#include <iostream>

#include <sys/ipc.h> 

#include <sys/msg.h> 

#include <yaml-cpp/yaml.h>

#include <jaegertracing/Tracer.h>

namespace{

struct msg_buffer { 
    long msg_type; 
    char msg_text[50]; 
} message; 

//Set up tracer and load config from YAML
void setUpTracer(const char* configFilePath)
{
    auto configYAML = YAML::LoadFile(configFilePath);
    auto config = jaegertracing::Config::parse(configYAML);
    auto tracer = jaegertracing::Tracer::make(
        "subscriber-service", config, jaegertracing::logging::consoleLogger());
    opentracing::Tracer::InitGlobal(
        std::static_pointer_cast<opentracing::Tracer>(tracer));
}

//Subcribe to the message via message queue
void subscribeFunction(const int messageID, const std::unique_ptr<opentracing::Span>& parentSpan)
{
    auto span = opentracing::Tracer::Global()->StartSpan(
        "tracedSubscribeSubroutine", { opentracing::ChildOf(&parentSpan->context()) });
    
    msgrcv(messageID, &message, sizeof(message), 1, 0);
    span->SetTag("message-recieved", message.msg_text);

    std::cout<<"Message recieved is: " << message.msg_text << std::endl;
    msgctl(messageID, IPC_RMID, NULL); 
    
}

//Set up the subscriber and message queue
void setUpSubscriber()
{
    auto span = opentracing::Tracer::Global()->StartSpan("setupSubscriberFunction");
    key_t key; 
    int msgid; 

    key = ftok("pub-sub", 65); 

    msgid = msgget(key, 0666 | IPC_CREAT);
    
    subscribeFunction(msgid, span);

}
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <config-yaml-path>\n";
        return 1;
    }

    setUpTracer(argv[1]);

    setUpSubscriber();

    opentracing::Tracer::Global()->Close();
    return 0;
}
