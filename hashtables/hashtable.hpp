#ifndef HASHTABLE_HPP
#define HASHTABLE_HPP

#include <cstddef>
#include <cstdint>

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

struct HMap
{
    HTab newer;
    HTab older;
    size_t migrate_pos = 0;
};

void h_init(HTab *htab, size_t n);
void h_insert(HTab *htab, HNode *node);
HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *));
HNode *h_detach(HTab *htab, HNode **from);

void hm_trigger_rehasing(HMap *hmap);
HNode *h_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_insert(HMap *hmap, HNode *node);

#endif // HASHTABLE_HPP