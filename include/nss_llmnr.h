/** *****************************************************
 * Interface that implements a NSS (name server switch) *
 * module for the protocol 'LLMNR' (see RFC 4795)       *
 ********************************************************/
 
#ifndef NSS_LLMNR_H
#define NSS_LLMNR_H

enum nss_status _nss_llmnr_gethostbyname_r(const char *name,
                                           struct hostent *result_buf,
                                           char *buf, size_t buflen,
                                           int *errnop, int *h_errnop);

enum nss_status _nss_llmnr_gethostbyname2_r (const char *name, int af,
                                             struct hostent *result_buf,
                                             char *buf, size_t buflen,
                                             int *errnop, int *h_errnop);

enum nss_status _nss_llmnr_gethostbyaddr_r(const void *addr, socklen_t len,
                                           int format, 
                                           struct hostent *result_buf,
                                           char *buffer, size_t buflen,
                                           int *errnop, int *h_errnop);
#endif
