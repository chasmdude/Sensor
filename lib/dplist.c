#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>
#include "dplist.h"

#define DPLIST_MEMORY_ERROR 1 // error due to mem alloc failure

#ifdef DEBUG
#define DEBUG_PRINTF(...) 									                                        \
        do {											                                            \
            fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	    \
            fprintf(stderr,__VA_ARGS__);								                            \
            fflush(stderr);                                                                         \
                } while(0)
#else
#define DEBUG_PRINTF(...) (void)0
#endif


#define DPLIST_ERR_HANDLER(condition, err_code)                         \
    do {                                                                \
            if ((condition)) DEBUG_PRINTF(#condition " failed\n");      \
            assert(!(condition));                                       \
        } while(0)


struct dplist {
    dplist_node_t *head;

    void *(*element_copy)(void *src_element);

    void (*element_free)(void **element);

    int (*element_compare)(void *x, void *y);
};

struct dplist_node {
    dplist_node_t *prev, *next;
    void *element;
};


dplist_t *dpl_create(
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) {
    dplist_t *list = (dplist_t *) malloc(sizeof(dplist_t));
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_MEMORY_ERROR);
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

void dpl_free(dplist_t **list, bool free_element) {
    if (list == NULL || *list == NULL) {
        return;
    }
    dplist_node_t *current = (*list)->head;
    while (current != NULL) {
        dplist_node_t *temp = current;
        current = current->next;

        if (free_element && (*list)->element_free != NULL) {
            ((*list)->element_free)(&(temp->element));
        }
        free(temp);
    }
    free(*list);
    *list = NULL;
}

int dpl_size(dplist_t *list) {
    if (list == NULL) {
        return -1;
    }

    int count = 0;
    dplist_node_t *current = list->head;
    while (current != NULL) {
        count++;
        current = current->next;
    }

    return count;
}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {
    dplist_node_t *list_node;
    if (list == NULL || element == NULL) {
        return NULL;
    }
    list_node = malloc(sizeof(dplist_node_t));
    DPLIST_ERR_HANDLER(list_node == NULL, DPLIST_MEMORY_ERROR);
    void *copy;
    if (insert_copy) {
        copy = list->element_copy(element);
        list_node->element = copy;
    } else {
        list_node->element = element;
    }
    if (list->head == NULL) {
        list_node->prev = NULL;
        list_node->next = NULL;
        list->head = list_node;
    } else if (index <= 0) {
        list_node->prev = NULL;
        list_node->next = list->head;
        list->head->prev = list_node;
        list->head = list_node;
    } else {
        dplist_node_t *ref_at_index;
        ref_at_index = dpl_get_reference_at_index(list, index);
        assert(ref_at_index != NULL);
        if (index < dpl_size(list)) {
            list_node->prev = ref_at_index->prev;
            list_node->next = ref_at_index;
            ref_at_index->prev->next = list_node;
            ref_at_index->prev = list_node;
        } else {
            assert(ref_at_index->next == NULL);
            list_node->next = NULL;
            list_node->prev = ref_at_index;
            ref_at_index->next = list_node;
        }
    }
    return list;
}

dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) {
    assert(list != NULL);

    int size = dpl_size(list);
    if (size == 0) {
        return list;
    }
    if (list == NULL || list->head == NULL) {
        return NULL;
    }

    dplist_node_t *ref_at_index = list->head;

    if (index <= 0) {
        if (free_element == true) {
            (list->element_free)(&(ref_at_index->element));
        }
        if (ref_at_index->next == NULL) {
            list->head = NULL;
        } else {
            list->head = ref_at_index->next;
            ref_at_index->next->prev = NULL;
            ref_at_index->next = NULL;
            ref_at_index->prev = NULL;
        }
    } else {
        ref_at_index = dpl_get_reference_at_index(list, index); //reference

        if (free_element == true) {
            (list->element_free)(&(ref_at_index->element));
        }

        if (index < (size - 1)) {
            ref_at_index->prev->next = ref_at_index->next;
            ref_at_index->next->prev = ref_at_index->prev;
            ref_at_index->next = NULL;
            ref_at_index->prev = NULL;
        } else { // node at the end of list
            if (size == 1) // 1 element in list
            {
                list->head = NULL;
            } else {
                ref_at_index->prev->next = NULL;
                ref_at_index->next = NULL;
                ref_at_index->prev = NULL;
            }
        }
    }
    ref_at_index->element = NULL;
    ref_at_index->next = NULL;
    ref_at_index->prev = NULL;
    free(ref_at_index);
    return list;
}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {
    if (list == NULL || list->head == NULL) return NULL;
    dplist_node_t *temp = list->head;
    if (index < 0)
        index = 0;
    size_t i = 0;
    while (i != index && temp->next != NULL) {
        i++;
        temp = temp->next;
    }
    return temp;
}

void *dpl_get_element_at_index(dplist_t *list, int index) {
    if (list == NULL || list->head == NULL)
        return 0;
    if (index < 0)
        index = 0;
    dplist_node_t *tempNode = list->head;
    int i = 0;
    while (i != index && tempNode->next != NULL) {
        i++;
        tempNode = tempNode->next;
    }
    return tempNode->element;
}
