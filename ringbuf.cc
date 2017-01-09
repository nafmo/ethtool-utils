#include <cstdio>
#include <cstring>

#include <unistd.h>

#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
       
int main(int argc, char *argv[])
{
   if (argc < 1)
   {
      fprintf(stderr, "Usage: %s dev ..\n", argv[0]);
      return 1;
   }

   /* Open a socket so that we can do ioctl()s */
   int fd = socket(AF_INET, SOCK_DGRAM, 0);
   if (fd < 0)
   {
      perror("socket");
      return 1;
   }

   ++ argv;
   while (argv && *argv)
   {
      const char *iface = *argv;
      printf("trying %s\n", iface);

      /* Set up command structure */
      ifreq ifr;
      memset(&ifr, 0, sizeof ifr);
      
      /* Select device */
      strcpy(ifr.ifr_name, iface);

      /* Select command */
      ethtool_ringparam ering;
      ifr.ifr_data =  reinterpret_cast<char *>(&ering);
      ering.cmd = ETHTOOL_GRINGPARAM; /* read ring data */
      int retval = ioctl(fd, SIOCETHTOOL, &ifr);
      if (retval < 0)
      {
         perror("ioctl");
      }
      else if (ering.rx_pending)
      {
         printf("Current rx value is %d\n", ering.rx_pending);

         if (ering.rx_pending < 4096)
         {
            ering.rx_pending = 4096;
            ering.cmd = ETHTOOL_SRINGPARAM;
            retval = ioctl(fd, SIOCETHTOOL, &ifr);
            if (retval < 0)
            {
               perror("ioctl 2");
            }
            else
            {
               printf(" ==> successfully updated to %d\n", ering.rx_pending);
            }
         }
      }
      else
      {
         printf("ioctl() did not return any data");
      }

      ++ argv;
   }
   close(fd);

   return 0;   
}