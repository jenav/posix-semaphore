#include "semaphore.h"
#include "typeinfo"

Nan::Persistent<v8::Function> Semaphore::constructor;

Semaphore::Semaphore(char buf[], bool create, char mask[], bool debug, bool retry_on_eintr, unsigned int value /*= 1*/)
{
  strcpy(this->sem_name, buf);
  if (create)
  {
    unsigned int bit_mask = strtol(mask, NULL, 8);
    this->semaphore = sem_open(this->sem_name, O_CREAT, bit_mask, value);
    if (this->semaphore == SEM_FAILED)
    {
      this->closed = 1;
      Nan::ThrowError("Could not create semaphore: sem_open failed");
      return ;
    }
  }
  else
  {
    this->semaphore = sem_open(this->sem_name, 0);
    if (this->semaphore == SEM_FAILED)
    {
      this->closed = 1;
      Nan::ThrowError("Could not open semaphore: sem_open failed");
      return ;
    }
  }
  this->closed = false;
  this->debug = debug;
  this->retry_on_eintr = retry_on_eintr;
}

Semaphore::~Semaphore()
{
}

void Semaphore::Init(v8::Local<v8::Object> exports)
{
  v8::Local<v8::Context> context = exports->CreationContext();
  Nan::HandleScope scope;

  // Prepare constructor template
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("Semaphore").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "wait", Wait);
  Nan::SetPrototypeMethod(tpl, "tryWait", TryWait);
  Nan::SetPrototypeMethod(tpl, "post", Post);
  Nan::SetPrototypeMethod(tpl, "close", Close);

  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  exports->Set(
    context,
    Nan::New("Semaphore").ToLocalChecked(),
    tpl->GetFunction(context).ToLocalChecked()
  ).FromJust();
}

void Semaphore::New(const Nan::FunctionCallbackInfo<v8::Value>& info)
{
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  bool create;
  bool debug;
  bool retry_on_eintr;
  char buf[SEMSIZE];
  char mask[5];
  unsigned int value;

  if (!info.IsConstructCall())
    return Nan::ThrowError("Must call Semaphore() with new");
  if ((info.Length() < 5) || (info.Length() > 6))
    return Nan::ThrowError("Semaphore() expects 4 or 5 arguments");
  if (!info[0]->IsString())   // name 
    return Nan::ThrowError("Semaphore() expects a string as first argument");
  if (!info[1]->IsBoolean())  // create
    return Nan::ThrowError("Semaphore() expects a boolean as second argument");
  if (!info[2]->IsString())   // bit mask
    return Nan::ThrowError("Semaphore() expects a string as third argument");
  if (!info[3]->IsBoolean())  // debug
    return Nan::ThrowError("Semaphore() expects a boolean as fourth argument");
  if (!info[4]->IsBoolean())  // retry_on_eintr
    return Nan::ThrowError("Semaphore() expects a boolean as fifth argument");
  if (!info[5]->IsUndefined() && !info[5]->IsUint32())  // value
    return Nan::ThrowError("Semaphore() expects an integer as sixth argument");
  
  create = Nan::To<bool>(info[1]).FromJust();
  debug = Nan::To<bool>(info[3]).FromJust();
  retry_on_eintr = Nan::To<bool>(info[4]).FromJust();
  value = !info[5]->IsUndefined()? info[5]->IntegerValue(context).FromJust(): 1;
  
  size_t str_len;
  // sem name
  v8::String::Utf8Value v8str(isolate, info[0]);
  std::string str(*v8str);
  str_len = str.length();
  if (str_len >= SEMSIZE - 1 || str_len <= 0)
    return Nan::ThrowError("Semaphore() : first argument's length must be < 255 && > 0");
  
  strncpy(buf, str.c_str(), str_len);
  buf[str_len] = '\0';
  
  // bit mask
  v8::String::Utf8Value v8str_mask(isolate, info[2]);
  std::string str_mask(*v8str_mask);
  str_len = str_mask.length();
  if (str_len > 4 || str_len <= 0)
    return Nan::ThrowError("Semaphore() : third argument's length must be <= 4 && > 0, ie: 0644 or 644");
  
  strncpy(mask, str_mask.c_str(), str_len);
  mask[str_len] = '\0';

  Semaphore* obj = new Semaphore(buf, create, mask, debug, retry_on_eintr, value);
  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

void Semaphore::Wait(const Nan::FunctionCallbackInfo<v8::Value>& info)
{
  int r;
  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(info.Holder());

  if (obj->closed)
    return Nan::ThrowError("trying to do operation over semaphore, but already closed");
  
  if (obj->debug)
    printf("[posix-semaphore] Waiting on semaphore\n");
  
  while ((r = sem_wait(obj->semaphore)) == -1 && errno == EINTR && obj->retry_on_eintr)
  {
    if (obj->debug)
      printf("[posix-semaphore] Got EINTR while calling sem_wait, retrying...\n");
  }
  // error and not EINTR
  if (r == -1)
  {
    if (obj->debug)
    {
      if (errno == EINTR)
      {
        printf("[posix-semaphore] Got EINTR while calling sem_wait, expected on SIGINT (CTRL-C). Not retrying because the 'retryOnEintr' option is false\n");
      }
      else
      {
        printf("[posix-semaphore] sem_wait failed, printing errno message ('man sem_wait' for more details on possible errors) : \n");
        perror("[posix-semaphore] ");
      }
    }
    if (errno != EINTR)
      return Nan::ThrowError("could not acquire semaphore, sem_wait failed");
  }
}

void Semaphore::TryWait(const Nan::FunctionCallbackInfo<v8::Value>& info)
{
  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(info.Holder());

  if (obj->closed)
    return Nan::ThrowError("trying to do operation over semaphore, but already closed");
  
  if (obj->debug)
    printf("[posix-semaphore] Trying to wait on semaphore\n");
  
  // let's try!
  if (sem_trywait(obj->semaphore) == -1)
  {
    if (obj->debug)
    {
      printf("[posix-semaphore] sem_trywait failed, printing errno message ('man sem_trywait' for more details on possible errors) : \n");
      perror("[posix-semaphore] ");
    }
    return Nan::ThrowError("could not acquire semaphore, sem_trywait failed");
  }
}

void Semaphore::Post(const Nan::FunctionCallbackInfo<v8::Value>& info)
{
  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(info.Holder());

  if (obj->closed)
    return Nan::ThrowError("trying to do operation over semaphore, but already closed");
  
  if (obj->debug)
    printf("[posix-semaphore] Posting semaphore\n");

  if (sem_post(obj->semaphore) == -1)
  {
    if (obj->debug)
    {
      printf("[posix-semaphore] sem_post failed, printing errno message ('man sem_post' for more details on possible errors): \n");
      perror("[posix-semaphore] ");
    }
    return Nan::ThrowError("could not release semaphore, sem_post failed");
  }
}

void Semaphore::Close(const Nan::FunctionCallbackInfo<v8::Value>& info)
{
  Semaphore* obj = ObjectWrap::Unwrap<Semaphore>(info.Holder());

  if (obj->closed)
    return Nan::ThrowError("trying to close semaphore, but already closed");
  
  if (obj->debug)
    printf("[posix-semaphore] Closing semaphore\n");
  
  obj->closed = true;
  sem_close(obj->semaphore);
}
