#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "dt.h"

void
queue_init (TaskQueue *q)
{
  q->head = q->tail = NULL;
  pthread_mutex_init (&q->mutex, NULL);
  pthread_cond_init (&q->cond, NULL);
  atomic_store (&q->count, 0);
  atomic_store (&q->shutdown, false);
}

void
queue_push (TaskQueue *q, const TaskItem *item)
{
  TaskNode *node = malloc (sizeof (TaskNode));
  if (!node)
    return;
  node->task = *item;
  node->next = NULL;
  pthread_mutex_lock (&q->mutex);
  if (!q->tail)
    q->head = node;
  else
    q->tail->next = node;
  q->tail = node;
  atomic_fetch_add (&q->count, 1);
  pthread_cond_signal (&q->cond);
  pthread_mutex_unlock (&q->mutex);
}

bool
queue_pop (TaskQueue *q, TaskItem *out_item)
{
  pthread_mutex_lock (&q->mutex);
  while (!q->head && !atomic_load (&q->shutdown))
    pthread_cond_wait (&q->cond, &q->mutex);
  if (!q->head && atomic_load (&q->shutdown))
    {
      pthread_mutex_unlock (&q->mutex);
      return false;
    }
  TaskNode *node = q->head;
  q->head = node->next;
  if (!q->head)
    q->tail = NULL;
  *out_item = node->task;
  free (node);
  atomic_fetch_sub (&q->count, 1);
  pthread_mutex_unlock (&q->mutex);
  return true;
}

void
queue_shutdown (TaskQueue *q)
{
  atomic_store (&q->shutdown, true);
  pthread_cond_broadcast (&q->cond);
}

void
queue_destroy (TaskQueue *q)
{
  queue_shutdown (q);
  TaskNode *cur = q->head;
  while (cur)
    {
      TaskNode *n = cur->next;
      if (cur->task.row_label)
	g_object_unref (cur->task.row_label);
      free (cur);
      cur = n;
    }
  pthread_mutex_destroy (&q->mutex);
  pthread_cond_destroy (&q->cond);
}
