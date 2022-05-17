#include <sys/types.h>
    #include <sys/socket.h>

    int main ()
    {
      int a = 0;
      struct sockaddr *b = 0;
      socklen_t *c = 0;
      accept (a, b, c);
      return 0;
   }
   