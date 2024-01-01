/**
 * @file util.h
 */

#pragma once

#include <sys/types.h>

#define MAXLINE 8192
#define LISTENQ 1024
extern char *message;

ssize_t rio_readn(int fd, void *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);