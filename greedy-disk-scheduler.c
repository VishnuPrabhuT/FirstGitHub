/*
 * elevator greedy
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

struct greedy_data {
   struct list_head lower_queue;
   struct list_head upper_queue;
   sector_t head_sector;   
};

/*
struct list_head* queue_select(struct sector entry_sector, struct greedy_data* greedy_data) {
   if(entry_sector < greedy_data->disk_head)
      return greedy_data->lower_queue;
   else
      return greedy_data->upper_queue;
}
*/

static void greedy_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

// dispatch upper
static struct request* greedy_next_lower(struct greedy_data* greedy_data) {
   return list_entry_rq(greedy_data->lower->next);
}

// dispatch lower
static struct request* greedy_next_upper(struct greedy_data* greedy_data) {
   return list_entry_rq(greedy_data->upper->prev);
}

static int greedy_dispatch(struct request_queue *q, int force) {
   struct request* rq, next_upper, next_lower;
	struct greedy_data *nd = q->elevator->elevator_data;
   struct sector lower_sector, upper_sector;

   if(list_empty(nd->lower_queue) && list_empty(nd->upper_queue)) return NULL;
   if(list_empty(nd->lower_queue)) { 
      rq = greedy_next_upper(q, nd);
      goto end;
   }
   if(list_empty(nd->upper_queue)) {
      rq = greedy_next_lower(q, nd);
      goto end;
   }
   
   next_lower = greedy_next_lower(nd);
   next_upper = greedy_next_upper(nd);

   lower_sector = blk_rq_pos(next_lower);
   upper_sector = blk_rq_pos(next_upper);

   if((upper_sector - nd->head_sector) < (nd->head_sector - lower_sector)) {
      rq = next_upper;
      goto end;
   }

   rq = next_lower;

   end:
   nd->head_sector = rq_end_sector(rq);
   list_del_init(&rq->queuelist);
   elv_dispatch_sort(q, rq);
   return 1;
}

static void greedy_sorted_add(struct list_head* entry, struct list_head* head) {
   struct request* entry_request = list_entry_rq(entry);
   struct sector   entry_sector  = blk_rq_pos(entry_request); 
   struct request* comparison;

   // to-do
   list_for_each_entry(comparison, head, queuelist) {
      if(entry_sector > blk_rq_pos(comparison)) {
          list_add_tail(entry, comparision->queuelist);
          return;
      }
   }
   list_add_tail(entry, head);
}

static void greedy_add_request(struct request_queue *q, struct request *rq) {
   struct greedy_data *nd = q->elevator->elevator_data;
   struct sector entry_sector = blk_rq_pos(rq);

   if(entry_sector < nd->head_sector)
      greedy_sorted_add(&rq->queuelist, &nd->lower_queue);
   else
      greedy_sorted_add(&rq->queuelist, &nd->upper_queue);
}

static struct request * greedy_former_request(struct request_queue *q, struct request *rq) {
	struct greedy_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->lower_queue || rq->queuelist.prev == &nd->upper_queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
greedy_latter_request(struct request_queue *q, struct request *rq)
{
	struct greedy_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->lower_queue || rq->queuelist.next == &nd->upper_queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

static int greedy_init_queue(struct request_queue *q, struct elevator_type *e) {
   struct greedy_data *nd;
   struct elevator_queue *eq;

   eq = elevator_alloc(q, e);
   if (!eq) return -ENOMEM;

   nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
   if (!nd) {
      kobject_put(&eq->kobj);
      return -ENOMEM;
   }
   eq->elevator_data = nd;

   INIT_LIST_HEAD(&nd->lower_queue);
   INIT_LIST_HEAD(&nd->upper_queue);
   nd->disk_head = 0ul;

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}

static void greedy_exit_queue(struct elevator_queue *e)
{
	struct greedy_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_greedy = {
	.ops = {
		.elevator_merge_req_fn		= greedy_merged_requests,
		.elevator_dispatch_fn		= greedy_dispatch,
		.elevator_add_req_fn		= greedy_add_request,
		.elevator_former_req_fn		= greedy_former_request,
		.elevator_latter_req_fn		= greedy_latter_request,
		.elevator_init_fn		= greedy_init_queue,
		.elevator_exit_fn		= greedy_exit_queue,
	},
	.elevator_name = "greedy",
	.elevator_owner = THIS_MODULE,
};

static int __init greedy_init(void)
{
	return elv_register(&elevator_greedy);
}

static void __exit greedy_exit(void)
{
	elv_unregister(&elevator_greedy);
}

module_init(greedy_init);
module_exit(greedy_exit);


MODULE_AUTHOR("Team eXtreme");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Greedy IO scheduler");
