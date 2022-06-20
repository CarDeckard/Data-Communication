#include <stdio.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

int main() {
    const char* data = "Hello World!";
    struct ip ip_header;
    struct udphdr udp_header;
    unsigned short totalDgramSize = sizeof(ip_header) + sizeof(udp_header) + strlen(data);

    ip_header.ip_v = 4;
    ip_header.ip_hl = 5;
    ip_header.ip_tos = 0;
    ip_header.ip_ttl = totalDgramSize;

    return 0;
}
