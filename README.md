
#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)


static inline void __list_add(struct list_head *_new,
		struct list_head *prev,
		struct list_head *next)
{
	next->prev = _new;
	_new->next = next;
	_new->prev = prev;
	prev->next = _new;
}

static inline void list_add(struct list_head *_new, struct list_head *head)
{
	__list_add(_new, head, head->next);
}

void* once_run(void* parm)  
{ 
  pthread_detach(pthread_self());
  
  int i = 0, cnt = (int)parm;
  for(i = 0; i < cnt; i++)
  {
    rtc_sess_t* rtc_sess = __rtc_sess_new();
    
    pthread_mutex_lock(&sess_list_lock);
    list_add(&rtc_sess->list, &sess_list);
    pthread_mutex_unlock(&sess_list_lock);
  }
  return NULL;
}  


rtc_init() 
    INIT_LIST_HEAD(&sess_list);
    return pthread_create(&once, NULL, once_run, (void*)SESS_MAX_CNT);




/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = LIST_POISON1; //0
	entry->prev = LIST_POISON2;  //0
}

rtc_sess_t *rtc_sess_new()
{
  rtc_sess_t *rtc_sess = NULL;
    printf("wuyaxiong,rtc_sess_new\n");
  
  pthread_mutex_lock(&sess_list_lock);
  
  if(!list_empty(&sess_list))
  {
    printf("wuyaxiong,!list_empty\n");
    rtc_sess = list_entry(sess_list.next, rtc_sess_t, list);
    list_del(&rtc_sess->list);
    sess_cnt++;
  }
...

