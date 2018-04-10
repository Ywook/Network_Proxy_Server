#define BUFFER_SIZE 1024
// #define MAX_ITEM_SIZE 10
#define MAX_CACHE_SIZE (5120*1024)
#define MAX_OBJECT_SIZE 524288
typedef struct LRU_LinkedList
{
    int size;
    int count;
    struct Node *header;
    struct Node *trailer;
} LRU;

typedef struct Node
{
    struct Node *next;
    struct Node *prev;
    char url[BUFFER_SIZE];
    char object[MAX_OBJECT_SIZE];
    int dataSize;
} Node;

void init(LRU *);
Node *newNode(char* , char*, int);
Node *search_url(LRU* , char* );
void addLast(LRU *, Node *);
Node *remove_First(LRU *);
void print_list(LRU *);


void init(LRU * list)
{
    list->header = NULL;
    list->trailer = NULL;
    list->size = 0;
    list->count = 0;
}

Node *newNode(char *url, char *object, int d_size)
{
    Node *new_Node = malloc(sizeof(Node));
    new_Node->next = new_Node->prev = NULL;
    memset(new_Node->url, '\0', BUFFER_SIZE);
    memset(new_Node->object, '\0', MAX_OBJECT_SIZE);
    memcpy(new_Node->url, url, BUFFER_SIZE);
    memcpy(new_Node->object, object, d_size);
    new_Node->dataSize = d_size;
    return new_Node;
}

Node *search_url(LRU * list, char *url)
{
    Node *temp;
    for (temp = list->header; temp != NULL; temp = temp->next)
    {
        if (strcmp(temp->url, url) == 0)
        {
            if (temp->next != NULL && temp != list->header)
            {
                Node *prevNode = temp->prev;

                prevNode->next = temp->next;
                temp->next->prev = prevNode;

                list->trailer->next = temp;
                temp->prev = list->trailer;
                temp->next = NULL;
                list->trailer = temp;
            }
            else if (temp == list->header)
            {
                if (list->count > 1)
                {
                    list->header = list->header->next;
                    list->header->prev = NULL;

                    temp->prev = list->trailer;
                    temp->next = NULL;

                    list->trailer->next = temp;
                    list->trailer = temp;
                }
            }
            return temp;
        }
    }
    return NULL;
}

void addLast(LRU * list, Node * new_node)
{
    if (list->count == 0)
    {
        list->header = list->trailer = new_node;
    }
    else
    {
        list->trailer->next = new_node;
        new_node->prev = list->trailer;
        new_node->next = NULL;
        list->trailer = new_node;
    }
    list->count = list->count + 1;
    list->size = list->size + new_node->dataSize;
    if (list->size > MAX_CACHE_SIZE)
    {
        while (list->size > MAX_CACHE_SIZE)
        {
            remove_First(list);
        }
    }
}

Node *remove_First(LRU * list)
{
    Node *temp = list->header;
    list->header = list->header->next;
    if (list->header == NULL)
    {
        list->trailer = NULL;
    }
    else
    {
        list->header->prev = NULL;
    }
    list->size = list->size - temp->dataSize;
    list->count = list->count - 1;
    return temp;
}

void print_list(LRU * list)
{
    Node *temp;
    int n = 1;
    printf("Current Cache Size : %i \n", list->size);
    for (temp = list->header; temp != NULL; temp = temp->next, n++)
    {
        printf("%d: %s %d\n", n, temp->url, temp->dataSize);
    }
}
