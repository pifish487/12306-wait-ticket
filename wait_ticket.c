#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define NAME_LEN 64
#define ID_LEN 32
#define OUTPUT_LEN 2048

/*
 * Source text and redirected output use UTF-8. On Windows, write UTF-16
 * directly to a real console so its active legacy code page cannot garble
 * Chinese text. This also works when the console is hosted by an IDE.
 */
static void write_utf8(const char *text) {
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    if (console != INVALID_HANDLE_VALUE && GetConsoleMode(console, &mode)) {
        int wide_len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
        if (wide_len > 0) {
            wchar_t *wide_text = (wchar_t *)malloc((size_t)wide_len * sizeof(wchar_t));
            if (wide_text != NULL) {
                if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, wide_len) > 0) {
                    DWORD written;
                    WriteConsoleW(console, wide_text, (DWORD)(wide_len - 1), &written, NULL);
                }
                free(wide_text);
                return;
            }
        }
    }
#endif
    fputs(text, stdout);
    fflush(stdout);
}

static int printf_utf8(const char *format, ...) {
    char buffer[OUTPUT_LEN];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length < 0) return length;
    write_utf8(buffer);
    return length;
}

static int puts_utf8(const char *text) {
    write_utf8(text);
    write_utf8("\n");
    return 0;
}

typedef struct Order {
    unsigned long long order_no;
    char passenger[NAME_LEN];
    char id_card[ID_LEN];
    int tickets_needed;
    struct Order *next;
} Order;

typedef struct {
    Order *front;
    Order *rear;
    size_t size;
} Queue;

static void queue_init(Queue *q) {
    q->front = q->rear = NULL;
    q->size = 0;
}

static int queue_empty(const Queue *q) {
    return q->front == NULL;
}

static void enqueue(Queue *q, Order *order) {
    order->next = NULL;
    if (q->rear == NULL) q->front = q->rear = order;
    else {
        q->rear->next = order;
        q->rear = order;
    }
    q->size++;
}

static Order *dequeue(Queue *q) {
    if (queue_empty(q)) return NULL;
    Order *order = q->front;
    q->front = order->next;
    if (q->front == NULL) q->rear = NULL;
    order->next = NULL;
    q->size--;
    return order;
}

static void clear_input_line(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

static int read_int(const char *prompt, int *value) {
    printf_utf8("%s", prompt);
    if (scanf("%d", value) != 1) {
        clear_input_line();
        return 0;
    }
    clear_input_line();
    return 1;
}

static void read_text(const char *prompt, char *buf, size_t capacity) {
    printf_utf8("%s", prompt);
    if (fgets(buf, (int)capacity, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
    else clear_input_line();
}

static void print_order(const Order *order) {
    printf_utf8("订单号：%llu | 旅客：%s | 身份证号：%s | 所需票数：%d\n",
                order->order_no, order->passenger, order->id_card,
                order->tickets_needed);
}

static void add_order(Queue *q, unsigned long long *next_order_no) {
    Order *order = (Order *)malloc(sizeof(*order));
    if (order == NULL) {
        puts_utf8("内存不足，添加失败。");
        return;
    }
    order->order_no = (*next_order_no)++;
    read_text("请输入旅客姓名：", order->passenger, sizeof(order->passenger));
    read_text("请输入身份证号：", order->id_card, sizeof(order->id_card));
    if (order->passenger[0] == '\0' || order->id_card[0] == '\0') {
        puts_utf8("姓名和身份证号不能为空，订单未加入队列。");
        free(order);
        return;
    }
    if (!read_int("请输入该订单所需票数：", &order->tickets_needed) ||
        order->tickets_needed <= 0) {
        puts_utf8("票数必须是正整数，订单未加入队列。");
        free(order);
        return;
    }
    enqueue(q, order);
    printf_utf8("订单已加入候补队列，当前排队数：%zu\n", q->size);
}

static void release_tickets(Queue *q) {
    int released;
    if (!read_int("请输入本次释放的余票数量：", &released) || released < 0) {
        puts_utf8("余票数量必须是非负整数。");
        return;
    }
    if (released == 0) {
        puts_utf8("本次没有释放余票。");
        return;
    }
    if (queue_empty(q)) {
        puts_utf8("候补队列为空，没有订单需要处理。");
        return;
    }

    while (q->front != NULL && released >= q->front->tickets_needed) {
        Order *order = dequeue(q);
        released -= order->tickets_needed;
        puts_utf8("候补成功，已出队：");
        print_order(order);
        free(order);
    }
    if (q->front != NULL && released > 0) {
        printf_utf8("剩余余票 %d 张，不足以满足队头订单（需要 %d 张）；订单继续等待。\n",
                    released, q->front->tickets_needed);
    }
    printf_utf8("当前候补订单数：%zu\n", q->size);
}

static void show_queue(const Queue *q) {
    if (queue_empty(q)) {
        puts_utf8("候补队列为空。");
        return;
    }
    printf_utf8("候补队列（共 %zu 单，按先来先服务）：\n", q->size);
    for (const Order *p = q->front; p != NULL; p = p->next) print_order(p);
}

static void free_queue(Queue *q) {
    Order *order;
    while ((order = dequeue(q)) != NULL) free(order);
}

int main(void) {
#ifdef _WIN32
    /* Keep console input consistent with the UTF-8 source file. */
    SetConsoleCP(CP_UTF8);
#endif

    Queue waiting;
    unsigned long long next_order_no = 1;
    int choice;
    queue_init(&waiting);

    for (;;) {
        puts_utf8("\n========== 12306 车票候补系统（基础版） ==========");
        puts_utf8("1. 新增候补订单");
        puts_utf8("2. 释放余票并处理候补");
        puts_utf8("3. 查看候补队列");
        puts_utf8("0. 退出程序");
        if (!read_int("请选择操作：", &choice)) {
            puts_utf8("输入无效，请输入菜单编号。");
            continue;
        }
        switch (choice) {
            case 1: add_order(&waiting, &next_order_no); break;
            case 2: release_tickets(&waiting); break;
            case 3: show_queue(&waiting); break;
            case 0:
                free_queue(&waiting);
                puts_utf8("程序已退出。");
                return 0;
            default: puts_utf8("没有该选项，请重新选择。");
        }
    }
}

