#include "assert.h"
#include "stddef.h"
#include "stdint.h"
#include <cstdlib>

const size_t k_max_load_factor = 8;
const size_t k_rehashing_work = 128;

struct HNode
{
    HNode *next = nullptr;
    uint64_t hcode = 0;
};

struct HTab
{
    HNode **tab = nullptr;
    size_t mask = 0;
    size_t size = 0;
};

// init Hashtable
static void h_init(HTab *htab, size_t n)
{
    assert((n > 0) && ((n - 1) && n) == 0); // assert if n is power of 2
    // this is pointer to pointer of HNode
    htab->tab = (HNode **)calloc(n, sizeof(HNode *));
    htab->mask = n - 1;
    htab->size = n;
}

static void h_insert(HTab *htab, HNode *node)
{
    size_t pos = htab->mask & node->hcode;
    // this is basically doing *(htab->tab + pos)
    // we want to insert at the top of the list
    HNode *next = htab->tab[pos];
    node->next = next;
    // update the top
    htab->tab[pos] = node;
    htab->size++;
}

// generic lookup
// returns the address of the parent NODE
// this allows us to do the lookup and also delete if needed
static HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *))
{
    if (!htab->tab)
    {
        return nullptr;
    }

    size_t pos = htab->mask && key->hcode;

    // we are taing the value of address of first node as a pointer, this is HEAD
    // of the linked list
    HNode **from = &htab->tab[pos];
    for (HNode *cur; (cur = *from) != nullptr; from = &cur->next)
    {
        if (cur->hcode == key->hcode && eq(cur, key))
        {
            return from;
        }
    }

    return nullptr;
}

static HNode *h_detach(HTab *htab, HNode **from)
{
    auto node = *from;
    from = &node->next;
    htab->size--;
    return node;
}

struct HMap
{
    HTab newer;
    HTab older;
    size_t migrate_pos = 0;
};

static void hm_trigger_rehasing(HMap *hmap)
{
    hmap->older = hmap->newer;
    h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
    hmap->migrate_pos = 0;
}

HNode *h_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *))
{
    HNode **from = h_lookup(&hmap->newer, key, eq);
    if (!from)
    {
        from = h_lookup(&hmap->older, key, eq);
    }
    return from ? *from : NULL;
}

HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *))
{
    if (HNode **from = h_lookup(&hmap->newer, key, eq))
    {
        return h_detach(&hmap->newer, from);
    }

    if (HNode **from = h_lookup(&hmap->older, key, eq))
    {
        return h_detach(&hmap->older, from);
    }
    return nullptr;
}

void hm_insert(HMap *hmap, HNode *node)
{
    if (!hmap->newer.tab)
    {
        h_init(&hmap->newer, 4);
    }
    h_insert(&hmap->newer, node);
    if (!hmap->older.tab) // if we dont have older yet
    {
        size_t shreshold = (hmap->newer.mask + 1) * k_max_load_factor;
        if (hmap->newer.size >= shreshold)
        {
            hm_trigger_rehasing(hmap);
        }
    }
    hm_help_rehashing(hmap);
}

static void hm_help_rehashing(HMap *hmap)
{
    size_t n_work = 0;
    // while we still have some work to do
    while (n_work < k_rehashing_work && hmap->older.size > 0)
    {
        HNode **from = &hmap->older.tab[hmap->migrate_pos];
        if (!from)
        {
            hmap->migrate_pos++;
            continue;
        }
        // move the first item to the newer table
        h_insert(&hmap->newer, h_detach(&hmap->older, from));
        n_work++;
    }
    if (hmap->older.size == 0 && hmap->older.tab)
    {
        free(hmap->older.tab);
        hmap->older = HTab{};
    }
}
