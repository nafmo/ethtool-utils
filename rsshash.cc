#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cctype>

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
      ethtool_rxfh erxfh = {0};
      ifr.ifr_data = reinterpret_cast<char *>(&erxfh);
      erxfh.cmd = ETHTOOL_GRSSH; /* Read RX flow hash */
      erxfh.rss_context = 0; /* Default */
      int retval = ioctl(fd, SIOCETHTOOL, &ifr);
      if (retval < 0)
      {
         perror("ioctl");
      }
      else if (erxfh.indir_size != 0 || erxfh.key_size != 0)
      {
         /* Set up structure to receive data */
         ethtool_rxfh *erxfhp = reinterpret_cast<ethtool_rxfh *>(std::calloc(1,
            sizeof (ethtool_rxfh)
            + erxfh.indir_size * sizeof (erxfh.rss_config[0])
            + erxfh.key_size));
         ifr.ifr_data = reinterpret_cast<char *>(erxfhp);

         erxfhp->cmd = ETHTOOL_GRSSH; /* Read RX flow hash */
         erxfhp->rss_context = 0;
         erxfhp->indir_size = erxfh.indir_size; /* Prepare to receive data */
         erxfhp->key_size = erxfh.key_size;

         retval = ioctl(fd, SIOCETHTOOL, &ifr);
         uint8_t* key = nullptr;
         if (retval < 0)
         {
            perror("ioctl 2");
         }
         else
         {
            /* We have now received the key; print it */
            printf("%s: ", iface);
            const char* delim = "";
            key =
               reinterpret_cast<uint8_t*>(erxfhp->rss_config)
               + (erxfhp->indir_size * sizeof (erxfhp->rss_config[0]));
            for (int i = 0; i < erxfhp->key_size; ++ i)
            {
               printf("%s%02x", delim, key[i]);
               delim = ":";
            }
            printf("\n");
         }

         /* If next parameter is a key, we want to replace the current key */
         const char *keystr = *(argv + 1);
         if (key && keystr && strlen(keystr) == 3 * erxfhp->key_size - 1)
         {
            bool valid_key = true;
            ++ argv; /* Consume parameter */
            erxfhp->cmd = ETHTOOL_SRSSH; /* Set RX flow hash */
            /* Keep the indirection table (will write it back, we could
             * consider using ETH_RXFH_INDIR_NO_CHANGE, but I am unsure
             * how to align the data in this case). */

            /* We know the length of the key is correct, now check
             * syntax */
            for (int i = 0; i < erxfhp->key_size; ++ i)
            {
               const char *p = keystr + (i * 3);
               if (std::isxdigit(*p) && std::isxdigit(p[1]) && (p[2] == ':' || p[2] == 0))
               {
                  unsigned long int val = std::strtoul(p, nullptr, 16); // must be [0,255]
                  key[i] = static_cast<uint8_t>(val);
               }
               else
               {
                  printf("Unable to parse RSS hash key %s at index %d\n", keystr, i * 3);
                  valid_key = false;
                  break;
               }
            }

            if (valid_key)
            {
               retval = ioctl(fd, SIOCETHTOOL, &ifr);
               if (retval < 0)
               {
                  perror("ioctl 3");
               }
               else
               {
                  printf("%s: RSS hash updated\n", iface);
               }
            }
         }

         std::free(erxfhp);
      }
      else
      {
          printf("ioctl() did not return any data\n");
      }

      ++ argv;
   }
   close(fd);

   return 0;
}
