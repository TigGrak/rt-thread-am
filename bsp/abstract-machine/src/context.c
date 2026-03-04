#include <am.h>
#include <klib.h>
#include <rtthread.h>

typedef struct stack_args {
  void (*tentry)(void *);
  void *parameter;
  void (*texit)(void);
} stack_args;

typedef struct switch_ctx_info {
  rt_ubase_t from;
  rt_ubase_t to;
}switch_ctx_info;

static void func_wrapper(void *arg)
{
  stack_args *args = (stack_args *)arg;
  args->tentry(args->parameter);
  args->texit();
  // RT-Thread会保证代码不会从texit中返回
  assert(0);
}

static Context* ev_handler(Event e, Context *c) {
  Context *ret = c;

  switch (e.event) {
    case EVENT_YIELD:
      {
        const rt_thread_t current = rt_thread_self();
        if (current && current->user_data != 0)
        {
          switch_ctx_info *info = (switch_ctx_info *)current->user_data;
          if (info->from != 0)
          {
            *(Context **)info->from = c;
          }
          if (info->to != 0)
          {
            ret = *(Context **)info->to;
          }
        }
        break;
      }

  case EVENT_IRQ_TIMER:
    ret = c;
    break;

    default: printf("Unhandled event ID = %d\n", e.event); assert(0);
  }
  return ret;
}

void __am_cte_init() {
  cte_init(ev_handler);
}

void rt_hw_context_switch_to(rt_ubase_t to) {
  rt_thread_t current = rt_thread_self();
  switch_ctx_info info;
  info.to = to;
  info.from = 0;
  current->user_data = (rt_ubase_t)&info;
  yield();

  // should not reach here
  assert(0);

}

void rt_hw_context_switch(rt_ubase_t from, rt_ubase_t to) {
  switch_ctx_info info;
  info.from = from;
  info.to   = to;
  rt_thread_t current = rt_thread_self();
  rt_ubase_t user_data_backup = current->user_data;
  current->user_data = (rt_ubase_t)&info;
  yield();
  current->user_data = user_data_backup;
}

void rt_hw_context_switch_interrupt(void *context, rt_ubase_t from, rt_ubase_t to, struct rt_thread *to_thread) {
  assert(0);
}

rt_uint8_t *rt_hw_stack_init(void *tentry, void *parameter, rt_uint8_t *stack_addr, void *texit) {
  uintptr_t stack_top = (uintptr_t)stack_addr;
  stack_top = stack_top & ~(16 - 1);// 16字节对齐

  /*
   *|----------| <- stack_top
   *|          |
   *|----------|
  */
  stack_top -= sizeof(stack_args);
  stack_args *args = (stack_args *)stack_top;
  /*
   *|----------|
   *|  args    |
   *|----------| <- stack_top
  */

  args->tentry = tentry;
  args->parameter = parameter;
  args->texit = texit;

  Area kstack;
  kstack.start = NULL;
  kstack.end = (void *)stack_top;
  Context *ctx = kcontext(kstack, func_wrapper, args);

  return (rt_uint8_t *)ctx;
  //assert(0);
  return NULL;
}
