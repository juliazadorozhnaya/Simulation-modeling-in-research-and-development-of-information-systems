#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <list>
#include <iostream>

using namespace std;

// Очень простой пример построения имитационной модели с календарём событий 
// Модель "самодостаточная" - не используются библиотеки для организации имитационного моделирования
// Возможности С++ используются недостаточно. 

class Event  // событие в календаре
{
public:
  float time; // время свершения события
  int type;   // тип события
  int attr; // дополнительные сведения о событии в зависимости от типа
  Event(float t, int tt, int a) {time = t; type = tt; attr = a;} 
};
// типы событий
#define EV_INIT 1
#define EV_REQ 2
#define EV_FIN 3
// состояния
#define RUN 1
#define IDLE 0

#define LIMIT 1000 // время окончания моделирования

class Request // задание в очереди
{
public:
  float time;     // время выполнения задания без прерываний 
  int source_num; // номер источника заданий (1 или 2)
  Request(float t, int s) {time = t; source_num = s;} 
};  

class Calendar: public list<Event*> // календарь событий
{
  public:
  void put (Event* ev); // вставить событие в список с упорядочением по полю time
  Event* get (); // извлечь первое событие из календаря (с наименьшим модельным временем)
};

void Calendar::put(Event *ev)
{
  list<Event*>::iterator i;
  Event ** e = new (Event*);
  *e = ev;
  if( empty() ){ push_back(*e); return; }
  i = begin();
  while((i != end()) && ((*i)->time <= ev->time)) 
  {
    ++i;
  }
  insert(i, *e);
} 
Event* Calendar::get()
{  
  if(empty()) return NULL; 
  Event *e = front(); 
  pop_front();
  return e;
}


typedef list<Request*> Queue; // очередь заданий к процессору 

float get_req_time(int source_num); // длительность задания
float get_pause_time(int source_num); // длительность паузы между заданиями


int main(int argc, char **argv )
{

Calendar calendar;
Queue queue;
float curr_time = 0;
Event *curr_ev;
float dt;
int cpu_state = IDLE;
float run_begin; // 
srand(2019);
// начальное событие и инициализация календаря
curr_ev = new Event(curr_time, EV_INIT, 0);
calendar.put( curr_ev );
// цикл по событиям

while((curr_ev = calendar.get()) != NULL )
{
  cout << "time " << curr_ev->time << " type " << curr_ev->type << endl;
  curr_time = curr_ev->time; // продвигаем время
  // обработка события
  if( curr_time >= LIMIT )break; // типичное дополнительное условие останова моделирования
  switch(curr_ev->type)
  {
  case EV_INIT:  // запускаем генераторы запросов
    calendar.put(new Event(curr_time, EV_REQ, 1));  
    calendar.put(new Event(curr_time, EV_REQ, 2));  
  break;
  case EV_REQ:
    // планируем событие окончания обработки, если процессор свободен, иначе ставим в очередь
    dt = get_req_time(curr_ev->attr); 
	cout << "dt " << dt << " num " << curr_ev->attr << endl;
    if(cpu_state == IDLE) 
	{ 
	  cpu_state = RUN; 
	  calendar.put(new Event(curr_time+dt, EV_FIN, curr_ev->attr)); 
	  run_begin = curr_time;
	} 
    else 
 	  queue.push_back(new Request(dt, curr_ev->attr));  
  // планируем событие генерации следующего задания
    calendar.put(new Event(curr_time+get_pause_time(curr_ev->attr), EV_REQ, curr_ev->attr)); 
	break;
  case EV_FIN:
    // объявляем процессор свободным и размещаем задание из очереди, если таковое есть
    cpu_state=IDLE; 
    // выводим запись о рабочем интервале
    cout << "Работа с " << run_begin << " по " << curr_time << " длит. " << (curr_time-run_begin) << endl; 
    if (!queue.empty()) 
    {
	  Request *rq = queue.front(); 
	  queue.pop_front(); 
	  calendar.put(new Event(curr_time+rq->time, EV_FIN, rq->source_num)); 
	  delete rq; 
	  run_begin = curr_time;
	} break;
  } // switch
   delete curr_ev;
} // while
} // main

int rc = 0; int pc = 0;
float get_req_time(int source_num)
{
// Для демонстрационных целей - выдаётся случайное значение
// при детализации модели функцию можно доработать
   double r = ((double)rand())/RAND_MAX;
   cout << "req " << rc << endl; rc++;
   if(source_num == 1) return r*10; else return r*20; 
}

float get_pause_time(int source_num) // длительность паузы между заданиями
{  
// см. комментарий выше
   double p = ((double)rand())/RAND_MAX;
   cout << "pause " << pc << endl; pc++;
   if(source_num == 1) return p*20; else return p*10; 
}

