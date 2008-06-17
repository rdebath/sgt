/*
 * protocol.c: Implementation of protocol.h.
 */

#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include <unistd.h>
#include <sys/socket.h>

#include "protocol.h"
#include "malloc.h"

struct protoread {
    unsigned char *data;
    size_t datalen, pktlen, datasize;
};

protoread *protoread_new(void)
{
    protoread *pr = snew(protoread);

    pr->datasize = 256;
    pr->data = snewn(pr->datasize, unsigned char);
    pr->datalen = 0;
    pr->pktlen = 4;

    return pr;
}

void protoread_free(protoread *pr)
{
    sfree(pr->data);
    sfree(pr);
}

void protoread_data(protoread *pr, void *vdata, size_t len,
		    protoread_pkt_fn_t gotpkt, void *ctx)
{
    unsigned char *data = (unsigned char *)vdata;

    while (len > 0) {
	size_t qty = pr->pktlen - pr->datalen;
	if (qty > len)
	    qty = len;
	memcpy(pr->data + pr->datalen, data, qty);
	data += qty;
	len -= qty;
	pr->datalen += qty;

	if (pr->pktlen == pr->datalen) {
	    if (pr->pktlen == 4) {
		pr->pktlen = pr->data[0];
		pr->pktlen = (pr->pktlen << 8) | pr->data[1];
		pr->pktlen = (pr->pktlen << 8) | pr->data[2];
		pr->pktlen = (pr->pktlen << 8) | pr->data[3];
		if (pr->datasize < pr->pktlen) {
		    pr->datasize = pr->pktlen;
		    pr->data = sresize(pr->data, pr->datasize, unsigned char);
		}
	    } else {
		gotpkt(ctx, pr->data[4], pr->data + 5, pr->pktlen - 5);
		pr->datalen = 0;
		pr->pktlen = 4;
	    }
	}
    }
}

size_t protowrite(sel_wfd *wfd, int type, const void *data1, ...)
{
    unsigned char header[5];
    size_t pktlen, ret;
    const void *data;
    int len;
    va_list ap;

    va_start(ap, data1);
    pktlen = 5;
    data = data1;
    while (data) {
	len = va_arg(ap, size_t);
	pktlen += len;
	data = va_arg(ap, const void *);
    }
    va_end(ap);

    header[4] = type;
    header[3] = pktlen & 0xFF; pktlen >>= 8;
    header[2] = pktlen & 0xFF; pktlen >>= 8;
    header[1] = pktlen & 0xFF; pktlen >>= 8;
    header[0] = pktlen & 0xFF;
    ret = sel_write(wfd, header, 5);

    va_start(ap, data1);
    pktlen = 0;
    data = data1;
    while (data) {
	len = va_arg(ap, size_t);
	ret = sel_write(wfd, data, len);
	data = va_arg(ap, const void *);
    }
    va_end(ap);

    return ret;
}

#define BUFLIMIT 65536

struct protocopy_encode {
    listnode ln;
    int infd, outfd;
    sel_rfd *rfd;
    sel_wfd *wfd;
    int flags;
};

static void protocopy_close_r(int fd, int flags)
{
    if (flags & PROTOCOPY_CLOSE_RFD)
	close(fd);
}

static void protocopy_close_w(int fd, int flags)
{
    if (flags & PROTOCOPY_CLOSE_WFD)
	close(fd);
    else if (flags & PROTOCOPY_SHUTDOWN_WFD)
	shutdown(fd, SHUT_WR);
}

static void protocopy_encode_written(sel_wfd *wfd, size_t bufsize)
{
    struct protocopy_encode *pe =
	(struct protocopy_encode *)sel_wfd_get_ctx(wfd);
    if (pe->rfd && bufsize < BUFLIMIT)
	sel_rfd_unfreeze(pe->rfd);
    if (!pe->rfd && bufsize == 0) {
	protocopy_close_w(sel_wfd_delete(pe->wfd), pe->flags);
	list_del((listnode *)pe);
	sfree(pe);
    }
}

static void protocopy_encode_writeerr(sel_wfd *wfd, int error)
{
    struct protocopy_encode *pe =
	(struct protocopy_encode *)sel_wfd_get_ctx(wfd);
    protocopy_close_r(sel_rfd_delete(pe->rfd), pe->flags);
    protocopy_close_w(sel_wfd_delete(pe->wfd), pe->flags);
    list_del((listnode *)pe);
    sfree(pe);
}

static void protocopy_encode_readdata(sel_rfd *rfd, void *data, size_t len)
{
    struct protocopy_encode *pe =
	(struct protocopy_encode *)sel_rfd_get_ctx(rfd);
    size_t bufsize;

    if (len == 0) {
	protocopy_close_r(sel_rfd_delete(pe->rfd), pe->flags);
	pe->rfd = NULL;
	bufsize = protowrite(pe->wfd, CMD_EOF, (void *)NULL);
	if (bufsize == 0) {
	    protocopy_close_w(sel_wfd_delete(pe->wfd), pe->flags);
	    list_del((listnode *)pe);
	    sfree(pe);
	}
    } else {
	int ret = protowrite(pe->wfd, CMD_DATA, data, len, (void *)NULL);
	if (ret > BUFLIMIT)
	    sel_rfd_freeze(pe->rfd);
    }
}

static void protocopy_encode_readerr(sel_rfd *rfd, int error)
{
    struct protocopy_encode *pe =
	(struct protocopy_encode *)sel_rfd_get_ctx(rfd);
    protocopy_close_r(sel_rfd_delete(pe->rfd), pe->flags);
    protocopy_close_w(sel_wfd_delete(pe->wfd), pe->flags);
    list_del((listnode *)pe);
    sfree(pe);
}

protocopy_encode *protocopy_encode_new(sel *sel, int infd, int outfd,
				       int flags)
{
    struct protocopy_encode *pe = snew(struct protocopy_encode);
    assert(infd >= 0);
    assert(outfd >= 0);
    pe->ln.type = LISTNODE_PROTOCOPY_ENCODE;
    pe->ln.list = NULL;
    pe->infd = infd;
    pe->outfd = outfd;
    pe->rfd = sel_rfd_add(sel, pe->infd, protocopy_encode_readdata,
			  protocopy_encode_readerr, pe);
    pe->wfd = sel_wfd_add(sel, pe->outfd, protocopy_encode_written,
			  protocopy_encode_writeerr, pe);
    pe->flags = flags;
    return pe;
}

void protocopy_encode_cutoff(protocopy_encode *pe)
{
    size_t bufsize;

    if (pe->rfd) {
	protocopy_close_r(sel_rfd_delete(pe->rfd), pe->flags);
	pe->rfd = NULL;
	bufsize = protowrite(pe->wfd, CMD_EOF, (void *)NULL);
	if (bufsize == 0) {
	    protocopy_close_w(sel_wfd_delete(pe->wfd), pe->flags);
	    list_del((listnode *)pe);
	    sfree(pe);
	}
    }
}

struct protocopy_decode {
    listnode ln;
    int infd, outfd;
    sel_rfd *rfd;
    sel_wfd *wfd;
    protoread *pr;
    int flags;
};

static void protocopy_decode_written(sel_wfd *wfd, size_t bufsize)
{
    struct protocopy_decode *pd =
	(struct protocopy_decode *)sel_wfd_get_ctx(wfd);
    if (pd->rfd && bufsize < BUFLIMIT)
	sel_rfd_unfreeze(pd->rfd);
    if (!pd->rfd && bufsize == 0) {
	protocopy_close_w(sel_wfd_delete(pd->wfd), pd->flags);
	list_del((listnode *)pd);
	sfree(pd);
    }
}

static void protocopy_decode_writeerr(sel_wfd *wfd, int error)
{
    struct protocopy_decode *pd =
	(struct protocopy_decode *)sel_wfd_get_ctx(wfd);
    protocopy_close_r(sel_rfd_delete(pd->rfd), pd->flags);
    protocopy_close_w(sel_wfd_delete(pd->wfd), pd->flags);
    list_del((listnode *)pd);
    sfree(pd);
}

static void protocopy_decode_packet(void *vctx, int type,
				    void *data, size_t len)
{
    struct protocopy_decode *pd =
	(struct protocopy_decode *)vctx;
    size_t bufsize;

    switch (type) {
      case CMD_DATA:
	bufsize = sel_write(pd->wfd, data, len);
	if (bufsize > BUFLIMIT)
	    sel_rfd_freeze(pd->rfd);
	break;
      case CMD_EOF:
	protocopy_close_r(sel_rfd_delete(pd->rfd), pd->flags);
	pd->rfd = NULL;
	bufsize = sel_write(pd->wfd, NULL, 0);
	if (bufsize == 0) {
	    protocopy_close_w(sel_wfd_delete(pd->wfd), pd->flags);
	    list_del((listnode *)pd);
	    sfree(pd);
	}
	break;
    }
}

static void protocopy_decode_readdata(sel_rfd *rfd, void *data, size_t len)
{
    struct protocopy_decode *pd =
	(struct protocopy_decode *)sel_rfd_get_ctx(rfd);
    protoread_data(pd->pr, data, len, protocopy_decode_packet, pd);
}

static void protocopy_decode_readerr(sel_rfd *rfd, int error)
{
    struct protocopy_decode *pd =
	(struct protocopy_decode *)sel_rfd_get_ctx(rfd);
    protocopy_close_r(sel_rfd_delete(pd->rfd), pd->flags);
    protocopy_close_w(sel_wfd_delete(pd->wfd), pd->flags);
    list_del((listnode *)pd);
    sfree(pd);
}

protocopy_decode *protocopy_decode_new(sel *sel, int infd, int outfd,
				       int flags)
{
    struct protocopy_decode *pd = snew(struct protocopy_decode);
    assert(infd >= 0);
    assert(outfd >= 0);
    pd->ln.type = LISTNODE_PROTOCOPY_DECODE;
    pd->ln.list = NULL;
    pd->infd = infd;
    pd->outfd = outfd;
    pd->rfd = sel_rfd_add(sel, pd->infd, protocopy_decode_readdata,
			  protocopy_decode_readerr, pd);
    pd->wfd = sel_wfd_add(sel, pd->outfd, protocopy_decode_written,
			  protocopy_decode_writeerr, pd);
    pd->pr = protoread_new();
    pd->flags = flags;
    return pd;
}

void protocopy_decode_destroy(protocopy_decode *pd)
{
    protocopy_close_r(sel_rfd_delete(pd->rfd), pd->flags);
    protocopy_close_w(sel_wfd_delete(pd->wfd), pd->flags);
    list_del((listnode *)pd);
    sfree(pd);
}

void list_init(list *list)
{
    list->head = list->tail = NULL;
}

void list_add(list *list, listnode *listnode)
{
    listnode->list = list;
    listnode->next = NULL;
    listnode->prev = list->tail;
    if (list->tail)
	list->tail->next = listnode;
    else
	list->head = listnode;
    list->tail = listnode;
}

void list_del(listnode *listnode)
{
    list *list = listnode->list;

    if (!list)
	return;

    if (listnode->prev)
	listnode->prev->next = listnode->next;
    else
	list->head = listnode->next;
    if (listnode->next)
	listnode->next->prev = listnode->prev;
    else
	list->tail = listnode->prev;
}
