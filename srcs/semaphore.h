#ifndef SEMAPHORE_H
#define SEMAPHORE_H
#define SEMSIZE 256

#include <nan.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>

class Semaphore : public Nan::ObjectWrap {
 public:
  static void Init(v8::Local<v8::Object> exports);

 private:
  explicit Semaphore(char *buf, bool create, char *mask, bool debug, bool retry_on_eintr, unsigned int value = 1);
  ~Semaphore();

  static void New(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static void Wait(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static void TryWait(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static void Post(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static void Close(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static void GetValue(const Nan::FunctionCallbackInfo<v8::Value>& info);
  static Nan::Persistent<v8::Function> constructor;

  sem_t	*semaphore;
  char	sem_name[SEMSIZE];
  bool	closed;
  bool debug;
  bool retry_on_eintr;
};

#endif
