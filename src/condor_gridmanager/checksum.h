#include "condor_common.h"



class CheckSum {
 public:
  CheckSum(void) { };
  virtual ~CheckSum(void) { };
  virtual void start(void) { };
  virtual void add(void* buf,unsigned long long int len) { };
  virtual void end(void) { };
  virtual void result(unsigned char*& res,unsigned int& len) const { len=0; };
  virtual int print(char* buf,int len) const { if(len>0) buf[0]=0; return 0; };
  virtual void scan(const char* buf) { };
  virtual operator bool(void) const { return false; };
  virtual bool operator!(void) const { return true; };
};


class CRC32Sum: public CheckSum {
 private:
  uint32_t r;
  unsigned long long count;
  bool computed;
 public:
  CRC32Sum(void);
  virtual ~CRC32Sum(void) { };
  virtual void start(void);
  virtual void add(void* buf,unsigned long long int len);
  virtual void end(void);
  virtual void result(unsigned char*& res,unsigned int& len) const { res=(unsigned char*)&r; len=4; };
  virtual int print(char* buf,int len) const;
  virtual void scan(const char* buf);
  virtual operator bool(void) const { return computed; };
  virtual bool operator!(void) const { return !computed; };
  uint32_t crc(void) const { return r; };
};
