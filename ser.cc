//
// (c) 2024 S-Patriarch
// Параллельный эхо-сервер.
//
#include "pl/tcpip.hh"
#include <unistd.h>
//------------------------------------------------------------------------------
#ifndef PL_LOGGER_HH
#include "pl/logger.hh"
#endif
#ifndef PL_DT_HH
#include "pl/dt.hh"
#endif
//------------------------------------------------------------------------------
#include <iostream>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <sys/wait.h>
#include <thread>
#include <functional>
#include <sstream>
//------------------------------------------------------------------------------
using SA = struct sockaddr;
//------------------------------------------------------------------------------
namespace {
   bool t_stop {false}; // флаг остановки потока
   std::string FILE_LOGGER {"slog"};
   //---------------------------------------------------------------------------
   struct {
      std::string _slog {""}; // строка записи
      bool _isfwlog {false};  // флаг возможности записи
   } log;
   //---------------------------------------------------------------------------
   pl::Logger l(FILE_LOGGER);
   pl::Dt     dt;
   pl::TCPip  tcp;
   //---------------------------------------------------------------------------
   void sig_chld(std::int32_t signo)
      // функция, корректно обрабатывающая сигнал SIGCHLD
   {
      std::int32_t stat {};
      pid_t pid {};
      // вызываем функцию waitpid (обработчик завершения дочернего процесса)
      // в цикле, получая состояние любого из дочерних процессов, которые 
      // завершились
      // WNOHANG - параметр, указывающий функции waitpid, что не нужно
      // блокироваться, если существует выполняемые дочерние процессы,
      // которые еще не завершились
      while ((pid = waitpid(-1,&stat,WNOHANG))>0) {
         std::cout << "W: child " << pid << " terminated.\n";
         std::ostringstream ss;
         ss << pid;
         log._slog = dt.get_date()+' '+dt.get_time()+" : "+
            "child "+ss.str()+" terminated";
         log._isfwlog = true;
     }
   }
   //---------------------------------------------------------------------------
   void* write_to_log_file(void* arg) 
      // функция потока, которая асинхронно основному потоку будет 
      // отписывать информацию в log-файл
   { 
      for (;;) {
         // проверяем на необходимость остановки потока
         if (t_stop) break;
         else {
            // проверяем на необходимость записи
            if (log._isfwlog) {
               l.write(log._slog);
               log._slog = "";
               log._isfwlog = false;
            }
         }
      }  
      return NULL;
   }
}
////////////////////////////////////////////////////////////////////////////////
std::int32_t main(std::int32_t argc, char** argv)
{
   std::cout << pl::mr::clrscr << pl::mr::crsh;

   pthread_t tid_log;
   ::tcp.tcp_pthread_create(&tid_log,NULL,::write_to_log_file,NULL);

   try {
      struct sockaddr_in saddr;
      struct sockaddr_in caddr;
 
      std::int32_t listenfd = ::tcp.tcp_socket(AF_INET,SOCK_STREAM,0);

      std::memset(&saddr,0,sizeof(saddr));
      saddr.sin_family = AF_INET;
      saddr.sin_port = htons(pl::mr::SERV_PORT);
      saddr.sin_addr.s_addr = htonl(INADDR_ANY);

      ::tcp.tcp_bind(listenfd,(SA*)&saddr,sizeof(saddr));
      ::tcp.tcp_listen(listenfd,pl::mr::LISTENQ);

      ::tcp.tcp_signal(SIGCHLD,sig_chld);

      std::cout << "W: the server is running...\n";
      ::log._slog = ::dt.get_date()+' '+::dt.get_time()+" : "+
         "the server is running";
      ::log._isfwlog = true;

      char buff[pl::mr::MAXLINE];
      for (;;) {
         std::int32_t connfd {};
         socklen_t len = sizeof(caddr);
         if ((connfd = accept(listenfd,(SA*)&caddr,&len))<0) {
            if (errno==EINTR) continue; 
            else {
               char errmsg[pl::mr::MAXLINE];
               strcpy(errmsg,"E: accept error - ");
               char* s = std::strerror(errno);
               throw pl::Exception(strcat(errmsg,s));
            }
         }
         std::cout << ::dt.get_date() << ' ' << ::dt.get_time()
                   << " connection from " 
                   << inet_ntop(AF_INET,&caddr.sin_addr,buff,sizeof(buff)) 
                   << ':' << ntohs(caddr.sin_port) << '\n';
         std::ostringstream ss;
         ss << ntohs(caddr.sin_port);
         ::log._slog = ::dt.get_date()+' '+::dt.get_time()+" : "+
            "connection from "+
            (std::string)inet_ntop(AF_INET,&caddr.sin_addr,buff,sizeof(buff))+
            ":"+ss.str();
         ::log._isfwlog = true;

         pid_t pid {};
         if ((pid = ::tcp.tcp_fork())==0) {
            ::tcp.tcp_close(listenfd); 
            std::int32_t n {};  
            char recvline[pl::mr::MAXLINE];
            for (;;) {
               std::memset(&recvline[0],0,sizeof(recvline));
               if ((n = ::tcp.tcp_read(connfd,recvline,pl::mr::MAXLINE))>0)
                  ::tcp.tcp_write(connfd,recvline,n);
               else break;
            }
            ::tcp.tcp_close(connfd); 
            std::exit(EXIT_SUCCESS);
         }
         ::tcp.tcp_close(connfd); 
      }
   }
   catch (std::exception& ex) {
      std::cout << ex.what() << '\n';
      ::t_stop = true;
   }

   ::tcp.tcp_pthread_join(tid_log,NULL);

   std::cout << pl::mr::crss;
   std::exit(EXIT_SUCCESS);
}
