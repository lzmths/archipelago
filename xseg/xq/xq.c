#include <xq/xq.h>
#include <xq/domain.h>

static inline int __snap(xqindex size)
{
	if (!size)
		return 0;
	return 1 << ((sizeof(size) * 8) - __builtin_clz(size) - 1);
}

void xq_free(struct xq *xq) {
	xq_mfree((void *)XPTR(&xq->queue));
	memset(xq, 0, sizeof(struct xq));
}

void xq_init_empty(struct xq *xq, xqindex size, void *mem)
{
	xq->head = 1;
	xq->tail = 0;
	XPTRSET(&xq->queue, mem);
	xq->size = __snap(size);
	xq_release(&xq->lock);
}

void xq_init_map(struct xq *xq,
		 xqindex size,
		 xqindex count,
		 xqindex (*mapfn)(xqindex),
		 void *mem)
{
	xqindex t, *qmem = mem;
	xq->head = count + 1;
	xq->tail = 0;
	XPTRSET(&xq->queue, qmem);
	xq->size = __snap(size);
	for (t = 0; t < count; t++)
		qmem[t] = mapfn(t);
	xq_release(&xq->lock);
}

void xq_init_seq(struct xq *xq, xqindex size, xqindex count, void *mem)
{
	xqindex t, *qmem = mem;
	xq->head = count + 1;
	xq->tail = 0;
	XPTRSET(&xq->queue, qmem);
	xq->size = __snap(size);
	for (t = 0; t < count; t++)
		qmem[t] = t;
	xq_release(&xq->lock);
}

xqindex *xq_alloc_empty(struct xq *xq, xqindex size)
{
	xqindex *mem = xq_malloc(size * sizeof(xqindex));
	if (!mem)
		return mem;
	xq_init_empty(xq, size, mem);
	return mem;
}

xqindex *xq_alloc_map(struct xq *xq,
			xqindex size,
			xqindex count,
			xqindex (*mapfn)(xqindex)	)
{
	xqindex *mem = xq_malloc(size * sizeof(xqindex));
	if (!mem)
		return mem;
	xq_init_map(xq, size, count, mapfn, mem);
	return mem;
}

xqindex *xq_alloc_seq(struct xq *xq, xqindex size, xqindex count)
{
	xqindex *mem = xq_malloc(size * sizeof(xqindex));
	if (!mem)
		return mem;
	xq_init_seq(xq, size, count, mem);
	return mem;
}

xqindex xq_size(struct xq *xq)
{
	return xq->size;
}

xqindex xq_count(struct xq *xq)
{
	return xq->head - xq->tail - 1;
}

xqindex xq_element(struct xq *xq, xqindex index)
{
	return XPTR(&xq->queue)[index & (xq->size - 1)];
}

void xq_print(struct xq *xq)
{
	xqindex i;

	XSEGLOG("xq head: %lu, tail: %lu, size: %lu\n",
		(unsigned long)xq->head,
		(unsigned long)xq->tail,
		(unsigned long)xq->size);
	i = xq->tail + 1;

	for (;;) {
		if (i == xq->head)
			break;
		XSEGLOG(	"%lu %lu\n",
			(unsigned long)i,
			(unsigned long)xq_element(xq, i) );
		i += 1;
	}
}

xqindex __xq_append_head(struct xq *xq, xqindex nr)
{
	xqindex head = xq->head;
	xq->head = head + nr;
	return head;
}

xqindex xq_append_heads(struct xq *xq,
			xqindex nr,
			xqindex *heads)
{
	xqindex i, mask, head;
	xqindex serial = xq_acquire(&xq->lock, nr);

	if (!(xq_count(xq) + nr <= xq->size)) {
		serial = Noneidx;
		goto out;
	}

	mask = xq->size -1;
	head = __xq_append_head(xq, nr);
	for (i = 0; i < nr; i++)
		XPTR(&xq->queue)[(head + i) & mask] = heads[i];
out:
	xq_release(&xq->lock);
	return serial;
}

xqindex xq_append_head(struct xq *xq, xqindex xqi)
{
	xqindex serial = xq_acquire(&xq->lock, 1);

	if (xq_count(xq) >= xq->size) {
		serial = Noneidx;
		goto out;
	}
	XPTR(&xq->queue)[__xq_append_head(xq, 1) & (xq->size -1)] = xqi;
out:
	xq_release(&xq->lock);
	return serial;
}

xqindex __xq_pop_head(struct xq *xq, xqindex nr)
{
	xqindex head = xq->head - nr;
	xq->head = head;
	return head;
}

xqindex xq_pop_heads(struct xq *xq,
			xqindex nr,
			xqindex *heads)
{
	xqindex i, mask, head;
	xqindex serial = xq_acquire(&xq->lock, nr);

	if (xq_count(xq) < nr) {
		serial = Noneidx;
		goto out;
	}

	mask = xq->size -1;
	head = __xq_pop_head(xq, nr);
	for (i = 0; i < nr; i++)
		heads[i] = XPTR(&xq->queue)[(head - i) & mask];
out:
	xq_release(&xq->lock);
	return serial;
}

xqindex xq_pop_head(struct xq *xq)
{
	xqindex value = Noneidx;
	(void)xq_acquire(&xq->lock, 1);
	if (!xq_count(xq))
		goto out;
	value = XPTR(&xq->queue)[__xq_pop_head(xq, 1) & (xq->size -1)];
out:
	xq_release(&xq->lock);
	return value;
}

xqindex __xq_append_tail(struct xq *xq, xqindex nr)
{
	xqindex tail = xq->tail - nr;
	xq->tail = tail;
	return tail + 1;
}

xqindex xq_append_tails(struct xq *xq,
			xqindex nr,
			xqindex *tails)
{
	xqindex i, mask, tail;
	xqindex serial = xq_acquire(&xq->lock, nr);

	if (!(xq_count(xq) + nr <= xq->size)) {
		serial = Noneidx;
		goto out;
	}

	mask = xq->size -1;
	tail = __xq_append_tail(xq, nr) + nr -1;
	for (i = 0; i < nr; i++)
		XPTR(&xq->queue)[(tail - i) & mask] = tails[i];
out:
	xq_release(&xq->lock);
	return serial;
}

xqindex xq_append_tail(struct xq *xq, xqindex xqi)
{
	xqindex serial = xq_acquire(&xq->lock, 1);

	if (!(xq_count(xq) + 1 <= xq->size)) {
		serial = Noneidx;
		goto out;
	}
	XPTR(&xq->queue)[__xq_append_tail(xq, 1) & (xq->size -1)] = xqi;
out:
	xq_release(&xq->lock);
	return serial;
}

xqindex __xq_pop_tail(struct xq *xq, xqindex nr)
{
	xqindex tail = xq->tail;
	xq->tail = tail + nr;
	return tail +1;
}

xqindex xq_pop_tails(struct xq *xq, xqindex nr, xqindex *tails)
{
	xqindex i, mask, tail;
	xqindex serial = xq_acquire(&xq->lock, nr);

	if (xq_count(xq) < nr) {
		serial = Noneidx;
		goto out;
	}

	mask = xq->size -1;
	tail = __xq_pop_tail(xq, nr);
	for (i = 0; i < nr; i++)
		tails[i] = XPTR(&xq->queue)[(tail + i) & mask];
out:
	xq_release(&xq->lock);
	return serial;
}

xqindex xq_pop_tail(struct xq *xq)
{
	xqindex value = Noneidx;
	(void)xq_acquire(&xq->lock, 1);
	if (!xq_count(xq))
		goto out;
	value = XPTR(&xq->queue)[__xq_pop_tail(xq, 1) & (xq->size -1)];
out:
	xq_release(&xq->lock);
	return value;
}

int xq_head_to_tail(struct xq *headq, struct xq *tailq, xqindex nr)
{
	xqindex head, tail, hmask, tmask, *hq, *tq, i, ret = -1;

	if (headq >= tailq) {
		xq_acquire(&headq->lock, nr);
		xq_acquire(&tailq->lock, nr);
	} else {
		xq_acquire(&tailq->lock, nr);
		xq_acquire(&headq->lock, nr);
	}

	if (xq_count(headq) < nr || xq_count(tailq) + nr > tailq->size)
		goto out;

	hmask = headq->size -1;
	tmask = tailq->size -1;
	head = __xq_pop_head(headq, nr);
	tail = __xq_append_tail(tailq, nr);
	hq = XPTR(&headq->queue);
	tq = XPTR(&tailq->queue);

	for (i = 0; i < nr; i++)
		tq[(tail + i) & tmask] = hq[(head + i) & hmask];

	ret = 0;
out:
	xq_release(&headq->lock);
	xq_release(&tailq->lock);
	return ret;
}

#ifdef __KERNEL__
#include <linux/module.h>
#include <xq/xq_exports.h>
#endif

