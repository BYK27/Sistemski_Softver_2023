#ifndef EXCEPTIONALERT_H
#define EXCEPTIONALERT_H

#include <exception>
#include <string>
using namespace std;


class ExceptionAlert: public exception{
private:
  string message;

public:
  ExceptionAlert(string m): message(m){}
  string get_message(){return this->message;}
};

#endif