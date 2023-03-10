#include "OpenSSLHelper.h"
#include <openssl/md5.h>
Buffer COpenSSLHelper::MD5(const Buffer& text)
{
    Buffer result;
    Buffer data(16);//md5Êä³ö
    MD5_CTX md5;
    MD5_Init(&md5);
    MD5_Update(&md5, (void*)text, text.size());
    MD5_Final(data, &md5);
    char temp[3] = "";
    for (size_t i = 0; i < data.size(); i++) {
        snprintf(temp, sizeof(temp), "%02x", data[i] & 0XFF);
        result += temp;
    }
    return result;
}
