

static qbs *qbs_malloc = (qbs *)calloc(sizeof(qbs) * 65536, 1); //~1MEG
static uint32 qbs_malloc_next = 0;                              // the next idex in qbs_malloc to use
static ptrszint *qbs_malloc_freed = (ptrszint *)malloc(ptrsz * 65536);
static uint32 qbs_malloc_freed_size = 65536;
static uint32 qbs_malloc_freed_num = 0; // number of freed qbs descriptors

/*MLP
    uint32 *dbglist=(uint32*)malloc(4*10000000);
    uint32 dbglisti=0;
    uint32 dbgline=0;
*/

qbs *qbs_new_descriptor() {
    // MLP //qbshlp1++;
    if (qbs_malloc_freed_num) {
        /*MLP
            static qbs *s;
            s=(qbs*)memset((void *)qbs_malloc_freed[--qbs_malloc_freed_num],0,sizeof(qbs));
            s->dbgl=dbgline;
            return s;
        */
        return (qbs *)memset((void *)qbs_malloc_freed[--qbs_malloc_freed_num], 0, sizeof(qbs));
    }
    if (qbs_malloc_next == 65536) {
        qbs_malloc = (qbs *)calloc(sizeof(qbs) * 65536, 1); //~1MEG
        qbs_malloc_next = 0;
    }
    /*MLP
        dbglist[dbglisti]=(uint32)&qbs_malloc[qbs_malloc_next];
        static qbs* s;
        s=(qbs*)&qbs_malloc[qbs_malloc_next++];
        s->dbgl=dbgline;
        dbglisti++;
        return s;
    */
    return &qbs_malloc[qbs_malloc_next++];
}

void qbs_free_descriptor(qbs *str) {
    // MLP //qbshlp1--;
    if (qbs_malloc_freed_num == qbs_malloc_freed_size) {
        qbs_malloc_freed_size *= 2;
        qbs_malloc_freed = (ptrszint *)realloc(qbs_malloc_freed, qbs_malloc_freed_size * ptrsz);
        if (!qbs_malloc_freed)
            error(508);
    }
    qbs_malloc_freed[qbs_malloc_freed_num] = (ptrszint)str;
    qbs_malloc_freed_num++;
    return;
}

// Used to track strings in 16bit memory
ptrszint *qbs_cmem_list = (ptrszint *)malloc(65536 * ptrsz);
uint32 qbs_cmem_list_lasti = 65535;
uint32 qbs_cmem_list_nexti = 0;
// Used to track strings in 32bit memory
ptrszint *qbs_list = (ptrszint *)malloc(65536 * ptrsz);
uint32 qbs_list_lasti = 65535;
uint32 qbs_list_nexti = 0;
// Used to track temporary strings for later removal when they fall out of scope
//*Some string functions delete a temporary string automatically after they have been
// passed one to save memory. In this case qbstring_templist[?]=0xFFFFFFFF
ptrszint *qbs_tmp_list = (ptrszint *)calloc(65536 * ptrsz, 1); // first index MUST be 0
uint32 qbs_tmp_list_lasti = 65535;
extern uint32 qbs_tmp_list_nexti;
// entended string memory

uint8 *qbs_data = (uint8 *)malloc(1048576);
uint32 qbs_data_size = 1048576;
uint32 qbs_sp = 0;

void field_free(qbs *str);

void qbs_free(qbs *str) {

    if (str->field)
        field_free(str);

    if (str->tmplisti) {
        qbs_tmp_list[str->tmplisti] = -1;
        while (qbs_tmp_list[qbs_tmp_list_nexti - 1] == -1) {
            qbs_tmp_list_nexti--;
        }
    }
    if (str->fixed || str->readonly) {
        qbs_free_descriptor(str);
        return;
    }
    if (str->in_cmem) {
        qbs_cmem_list[str->listi] = -1;
        if ((qbs_cmem_list_nexti - 1) == str->listi)
            qbs_cmem_list_nexti--;
    } else {
        qbs_list[str->listi] = -1;
    retry:
        if (qbs_list[qbs_list_nexti - 1] == -1) {
            qbs_list_nexti--;
            if (qbs_list_nexti)
                goto retry;
        }
        if (qbs_list_nexti) {
            qbs_sp = ((qbs *)qbs_list[qbs_list_nexti - 1])->chr - qbs_data + ((qbs *)qbs_list[qbs_list_nexti - 1])->len + 32;
            if (qbs_sp > qbs_data_size)
                qbs_sp = qbs_data_size; // adding 32 could overflow buffer!
        } else {
            qbs_sp = 0;
        }
    }
    qbs_free_descriptor(str);
    return;
}

void qbs_cmem_concat_list() {
    uint32 i;
    uint32 d;
    qbs *tqbs;
    d = 0;
    for (i = 0; i < qbs_cmem_list_nexti; i++) {
        if (qbs_cmem_list[i] != -1) {
            if (i != d) {
                tqbs = (qbs *)qbs_cmem_list[i];
                tqbs->listi = d;
                qbs_cmem_list[d] = (ptrszint)tqbs;
            }
            d++;
        }
    }
    qbs_cmem_list_nexti = d;
    // if string listings are taking up more than half of the list array double the list array's size
    if (qbs_cmem_list_nexti >= (qbs_cmem_list_lasti / 2)) {
        qbs_cmem_list_lasti *= 2;
        qbs_cmem_list = (ptrszint *)realloc(qbs_cmem_list, (qbs_cmem_list_lasti + 1) * ptrsz);
        if (!qbs_cmem_list)
            error(509);
    }
    return;
}

void qbs_concat_list() {
    uint32 i;
    uint32 d;
    qbs *tqbs;
    d = 0;
    for (i = 0; i < qbs_list_nexti; i++) {
        if (qbs_list[i] != -1) {
            if (i != d) {
                tqbs = (qbs *)qbs_list[i];
                tqbs->listi = d;
                qbs_list[d] = (ptrszint)tqbs;
            }
            d++;
        }
    }
    qbs_list_nexti = d;
    // if string listings are taking up more than half of the list array double the list array's size
    if (qbs_list_nexti >= (qbs_list_lasti / 2)) {
        qbs_list_lasti *= 2;
        qbs_list = (ptrszint *)realloc(qbs_list, (qbs_list_lasti + 1) * ptrsz);
        if (!qbs_list)
            error(510);
    }
    return;
}

void qbs_tmp_concat_list() {
    if (qbs_tmp_list_nexti >= (qbs_tmp_list_lasti / 2)) {
        qbs_tmp_list_lasti *= 2;
        qbs_tmp_list = (ptrszint *)realloc(qbs_tmp_list, (qbs_tmp_list_lasti + 1) * ptrsz);
        if (!qbs_tmp_list)
            error(511);
    }
    return;
}

void qbs_concat(uint32 bytesrequired) {
    // this does not change indexing, only ->chr pointers and the location of their data
    static int32 i;
    static uint8 *dest;
    static qbs *tqbs;
    dest = (uint8 *)qbs_data;
    if (qbs_list_nexti) {
        qbs_sp = 0;
        for (i = 0; i < qbs_list_nexti; i++) {
            if (qbs_list[i] != -1) {
                tqbs = (qbs *)qbs_list[i];
                if ((tqbs->chr - dest) > 32) {
                    if (tqbs->len) {
                        memmove(dest, tqbs->chr, tqbs->len);
                    }
                    tqbs->chr = dest;
                }
                dest = tqbs->chr + tqbs->len;
                qbs_sp = dest - qbs_data;
            }
        }
    }

    if (((qbs_sp * 2) + (bytesrequired + 32)) >= qbs_data_size) {
        static uint8 *oldbase;
        oldbase = qbs_data;
        qbs_data_size = qbs_data_size * 2 + bytesrequired;
        qbs_data = (uint8 *)realloc(qbs_data, qbs_data_size);
        if (qbs_data == NULL)
            error(512); // realloc failed!
        for (i = 0; i < qbs_list_nexti; i++) {
            if (qbs_list[i] != -1) {
                tqbs = (qbs *)qbs_list[i];
                tqbs->chr = tqbs->chr - oldbase + qbs_data;
            }
        }
    }
    return;
}

// as the cmem stack has a limit if bytesrequired cannot be met this exits and returns an error
// the cmem stack cannot after all be extended!
// so bytesrequired is only passed to possibly generate an error, or not generate one
void qbs_concat_cmem(uint32 bytesrequired) {
    // this does not change indexing, only ->chr pointers and the location of their data
    int32 i;
    uint8 *dest;
    qbs *tqbs;
    dest = (uint8 *)dblock;
    qbs_cmem_sp = qbs_cmem_descriptor_space;
    if (qbs_cmem_list_nexti) {
        for (i = 0; i < qbs_cmem_list_nexti; i++) {
            if (qbs_cmem_list[i] != -1) {
                tqbs = (qbs *)qbs_cmem_list[i];
                if (tqbs->chr != dest) {
                    if (tqbs->len) {
                        memmove(dest, tqbs->chr, tqbs->len);
                    }
                    tqbs->chr = dest;
                    // update cmem_descriptor [length][offset]
                    if (tqbs->cmem_descriptor) {
                        tqbs->cmem_descriptor[0] = tqbs->len;
                        tqbs->cmem_descriptor[1] = (uint16)(ptrszint)(tqbs->chr - dblock);
                    }
                }
                dest += tqbs->len;
                qbs_cmem_sp += tqbs->len;
            }
        }
    }
    if ((qbs_cmem_sp + bytesrequired) > cmem_sp)
        error(513);
    return;
}

qbs *qbs_new_cmem(int32 size, uint8 tmp) {
    if ((qbs_cmem_sp + size) > cmem_sp)
        qbs_concat_cmem(size);
    qbs *newstr;
    newstr = qbs_new_descriptor();
    newstr->len = size;
    if ((qbs_cmem_sp + size) > cmem_sp)
        qbs_concat_cmem(size);
    newstr->chr = (uint8 *)dblock + qbs_cmem_sp;
    qbs_cmem_sp += size;
    newstr->in_cmem = 1;
    if (qbs_cmem_list_nexti > qbs_cmem_list_lasti)
        qbs_cmem_concat_list();
    newstr->listi = qbs_cmem_list_nexti;
    qbs_cmem_list[newstr->listi] = (ptrszint)newstr;
    qbs_cmem_list_nexti++;
    if (tmp) {
        if (qbs_tmp_list_nexti > qbs_tmp_list_lasti)
            qbs_tmp_concat_list();
        newstr->tmplisti = qbs_tmp_list_nexti;
        qbs_tmp_list[newstr->tmplisti] = (ptrszint)newstr;
        qbs_tmp_list_nexti++;
        newstr->tmp = 1;
    } else {
        // alloc string descriptor in DBLOCK (4 bytes)
        cmem_sp -= 4;
        newstr->cmem_descriptor = (uint16 *)(dblock + cmem_sp);
        if (cmem_sp < qbs_cmem_sp)
            error(514);
        newstr->cmem_descriptor_offset = cmem_sp;
        // update cmem_descriptor [length][offset]
        newstr->cmem_descriptor[0] = newstr->len;
        newstr->cmem_descriptor[1] = (uint16)(ptrszint)(newstr->chr - dblock);
    }
    return newstr;
}

qbs *qbs_new(int32, uint8);

qbs *qbs_new_txt(const char *txt) {
    qbs *newstr;
    newstr = qbs_new_descriptor();
    if (!txt) { // NULL pointer is converted to a 0-length string
        newstr->len = 0;
    } else {
        newstr->len = strlen(txt);
    }
    newstr->chr = (uint8 *)txt;
    if (qbs_tmp_list_nexti > qbs_tmp_list_lasti)
        qbs_tmp_concat_list();
    newstr->tmplisti = qbs_tmp_list_nexti;
    qbs_tmp_list[newstr->tmplisti] = (ptrszint)newstr;
    qbs_tmp_list_nexti++;
    newstr->tmp = 1;
    newstr->readonly = 1;
    return newstr;
}

qbs *qbs_new_txt_len(const char *txt, int32 len) {
    qbs *newstr;
    newstr = qbs_new_descriptor();
    newstr->len = len;
    newstr->chr = (uint8 *)txt;
    if (qbs_tmp_list_nexti > qbs_tmp_list_lasti)
        qbs_tmp_concat_list();
    newstr->tmplisti = qbs_tmp_list_nexti;
    qbs_tmp_list[newstr->tmplisti] = (ptrszint)newstr;
    qbs_tmp_list_nexti++;
    newstr->tmp = 1;
    newstr->readonly = 1;
    return newstr;
}

// note: qbs_new_fixed detects if string is in DBLOCK
qbs *qbs_new_fixed(uint8 *offset, uint32 size, uint8 tmp) {
    qbs *newstr;
    newstr = qbs_new_descriptor();
    newstr->len = size;
    newstr->chr = offset;
    newstr->fixed = 1;
    if (tmp) {
        if (qbs_tmp_list_nexti > qbs_tmp_list_lasti)
            qbs_tmp_concat_list();
        newstr->tmplisti = qbs_tmp_list_nexti;
        qbs_tmp_list[newstr->tmplisti] = (ptrszint)newstr;
        qbs_tmp_list_nexti++;
        newstr->tmp = 1;
    } else {
        // is it in DBLOCK?
        if ((offset > (cmem + 1280)) && (offset < (cmem + 66816))) {
            // alloc string descriptor in DBLOCK (4 bytes)
            cmem_sp -= 4;
            newstr->cmem_descriptor = (uint16 *)(dblock + cmem_sp);
            if (cmem_sp < qbs_cmem_sp)
                error(515);
            newstr->cmem_descriptor_offset = cmem_sp;
            // update cmem_descriptor [length][offset]
            newstr->cmem_descriptor[0] = newstr->len;
            newstr->cmem_descriptor[1] = (uint16)(ptrszint)(newstr->chr - dblock);
        }
    }
    return newstr;
}

qbs *qbs_new(int32 size, uint8 tmp) {
    static qbs *newstr;
    if ((qbs_sp + size + 32) > qbs_data_size)
        qbs_concat(size + 32);
    newstr = qbs_new_descriptor();
    newstr->len = size;
    newstr->chr = qbs_data + qbs_sp;
    qbs_sp += size + 32;
    if (qbs_list_nexti > qbs_list_lasti)
        qbs_concat_list();
    newstr->listi = qbs_list_nexti;
    qbs_list[newstr->listi] = (ptrszint)newstr;
    qbs_list_nexti++;
    if (tmp) {
        if (qbs_tmp_list_nexti > qbs_tmp_list_lasti)
            qbs_tmp_concat_list();
        newstr->tmplisti = qbs_tmp_list_nexti;
        qbs_tmp_list[newstr->tmplisti] = (ptrszint)newstr;
        qbs_tmp_list_nexti++;
        newstr->tmp = 1;
    }
    return newstr;
}

void qbs_maketmp(qbs *str) {
    // WARNING: assumes str is a non-tmp string in non-cmem
    if (qbs_tmp_list_nexti > qbs_tmp_list_lasti)
        qbs_tmp_concat_list();
    str->tmplisti = qbs_tmp_list_nexti;
    qbs_tmp_list[str->tmplisti] = (ptrszint)str;
    qbs_tmp_list_nexti++;
    str->tmp = 1;
}

qbs *qbs_set(qbs *deststr, qbs *srcstr) {
    int32 i;
    qbs *tqbs;
    // fixed deststr
    if (deststr->fixed) {
        if (srcstr->len >= deststr->len) {
            memcpy(deststr->chr, srcstr->chr, deststr->len);
        } else {
            memcpy(deststr->chr, srcstr->chr, srcstr->len);
            memset(deststr->chr + srcstr->len, 32, deststr->len - srcstr->len); // pad with spaces
        }
        goto qbs_set_return;
    }
    // non-fixed deststr

    // can srcstr be aquired by deststr?
    if (srcstr->tmp) {
        if (srcstr->fixed == 0) {
            if (srcstr->readonly == 0) {
                if (srcstr->in_cmem == deststr->in_cmem) {
                    if (deststr->in_cmem) {
                        // unlist deststr and acquire srcstr's list index
                        qbs_cmem_list[deststr->listi] = -1;
                        qbs_cmem_list[srcstr->listi] = (ptrszint)deststr;
                        deststr->listi = srcstr->listi;
                    } else {
                        // unlist deststr and acquire srcstr's list index
                        qbs_list[deststr->listi] = -1;
                        qbs_list[srcstr->listi] = (ptrszint)deststr;
                        deststr->listi = srcstr->listi;
                    }

                    qbs_tmp_list[srcstr->tmplisti] = -1;
                    if (srcstr->tmplisti == (qbs_tmp_list_nexti - 1))
                        qbs_tmp_list_nexti--; // correct last tmp index for performance

                    deststr->chr = srcstr->chr;
                    deststr->len = srcstr->len;
                    qbs_free_descriptor(srcstr);
                    // update cmem_descriptor [length][offset]
                    if (deststr->cmem_descriptor) {
                        deststr->cmem_descriptor[0] = deststr->len;
                        deststr->cmem_descriptor[1] = (uint16)(ptrszint)(deststr->chr - dblock);
                    }
                    return deststr; // nb. This return cannot be changed to a goto qbs_set_return!
                }
            }
        }
    }

    // srcstr is equal length or shorter
    if (srcstr->len <= deststr->len) {
        memcpy(deststr->chr, srcstr->chr, srcstr->len);
        deststr->len = srcstr->len;
        goto qbs_set_return;
    }

    // srcstr is longer
    if (deststr->in_cmem) {
        if (deststr->listi == (qbs_cmem_list_nexti - 1)) {                      // last index
            if (((ptrszint)deststr->chr + srcstr->len) <= (dblock + cmem_sp)) { // space available
                memcpy(deststr->chr, srcstr->chr, srcstr->len);
                deststr->len = srcstr->len;
                qbs_cmem_sp = ((ptrszint)deststr->chr) + (ptrszint)deststr->len - dblock;
                goto qbs_set_return;
            }
            goto qbs_set_cmem_concat_required;
        }
        // deststr is not the last index so locate next valid index
        i = deststr->listi + 1;
    qbs_set_nextindex:
        if (qbs_cmem_list[i] != -1) {
            tqbs = (qbs *)qbs_cmem_list[i];
            if (tqbs == srcstr) {
                if (srcstr->tmp == 1)
                    goto skippedtmpsrcindex;
            }
            if ((deststr->chr + srcstr->len) > tqbs->chr)
                goto qbs_set_cmem_concat_required;
            memcpy(deststr->chr, srcstr->chr, srcstr->len);
            deststr->len = srcstr->len;
            goto qbs_set_return;
        }
    skippedtmpsrcindex:
        i++;
        if (i != qbs_cmem_list_nexti)
            goto qbs_set_nextindex;
        // all next indexes invalid!
        qbs_cmem_list_nexti = deststr->listi + 1;                           // adjust nexti
        if (((ptrszint)deststr->chr + srcstr->len) <= (dblock + cmem_sp)) { // space available
            memmove(deststr->chr, srcstr->chr, srcstr->len);                // overlap possible due to sometimes aquiring srcstr's space
            deststr->len = srcstr->len;
            qbs_cmem_sp = ((ptrszint)deststr->chr) + (ptrszint)deststr->len - dblock;
            goto qbs_set_return;
        }
    qbs_set_cmem_concat_required:
        // srcstr could not fit in deststr
        //"realloc" deststr
        qbs_cmem_list[deststr->listi] = -1;          // unlist
        if ((qbs_cmem_sp + srcstr->len) > cmem_sp) { // must concat!
            qbs_concat_cmem(srcstr->len);
        }
        if (qbs_cmem_list_nexti > qbs_cmem_list_lasti)
            qbs_cmem_concat_list();
        deststr->listi = qbs_cmem_list_nexti;
        qbs_cmem_list[qbs_cmem_list_nexti] = (ptrszint)deststr;
        qbs_cmem_list_nexti++; // relist
        deststr->chr = (uint8 *)dblock + qbs_cmem_sp;
        deststr->len = srcstr->len;
        qbs_cmem_sp += deststr->len;
        memcpy(deststr->chr, srcstr->chr, srcstr->len);
        goto qbs_set_return;
    }

    // not in cmem
    if (deststr->listi == (qbs_list_nexti - 1)) {                                             // last index
        if (((ptrszint)deststr->chr + srcstr->len) <= ((ptrszint)qbs_data + qbs_data_size)) { // space available
            memcpy(deststr->chr, srcstr->chr, srcstr->len);
            deststr->len = srcstr->len;
            qbs_sp = ((ptrszint)deststr->chr) + (ptrszint)deststr->len - (ptrszint)qbs_data;
            goto qbs_set_return;
        }
        goto qbs_set_concat_required;
    }
    // deststr is not the last index so locate next valid index
    i = deststr->listi + 1;
qbs_set_nextindex2:
    if (qbs_list[i] != -1) {
        tqbs = (qbs *)qbs_list[i];
        if (tqbs == srcstr) {
            if (srcstr->tmp == 1)
                goto skippedtmpsrcindex2;
        }
        if ((deststr->chr + srcstr->len) > tqbs->chr)
            goto qbs_set_concat_required;
        memcpy(deststr->chr, srcstr->chr, srcstr->len);
        deststr->len = srcstr->len;
        goto qbs_set_return;
    }
skippedtmpsrcindex2:
    i++;
    if (i != qbs_list_nexti)
        goto qbs_set_nextindex2;
    // all next indexes invalid!

    qbs_list_nexti = deststr->listi + 1;                                                  // adjust nexti
    if (((ptrszint)deststr->chr + srcstr->len) <= ((ptrszint)qbs_data + qbs_data_size)) { // space available
        memmove(deststr->chr, srcstr->chr, srcstr->len);                                  // overlap possible due to sometimes aquiring srcstr's space
        deststr->len = srcstr->len;
        qbs_sp = ((ptrszint)deststr->chr) + (ptrszint)deststr->len - (ptrszint)qbs_data;
        goto qbs_set_return;
    }

qbs_set_concat_required:
    // srcstr could not fit in deststr
    //"realloc" deststr
    qbs_list[deststr->listi] = -1;                // unlist
    if ((qbs_sp + srcstr->len) > qbs_data_size) { // must concat!
        qbs_concat(srcstr->len);
    }
    if (qbs_list_nexti > qbs_list_lasti)
        qbs_concat_list();
    deststr->listi = qbs_list_nexti;
    qbs_list[qbs_list_nexti] = (ptrszint)deststr;
    qbs_list_nexti++; // relist

    deststr->chr = qbs_data + qbs_sp;
    deststr->len = srcstr->len;
    qbs_sp += deststr->len;
    memcpy(deststr->chr, srcstr->chr, srcstr->len);

//(fall through to qbs_set_return)
qbs_set_return:
    if (srcstr->tmp) { // remove srcstr if it is a tmp string
        qbs_free(srcstr);
    }
    // update cmem_descriptor [length][offset]
    if (deststr->cmem_descriptor) {
        deststr->cmem_descriptor[0] = deststr->len;
        deststr->cmem_descriptor[1] = (uint16)(ptrszint)(deststr->chr - dblock);
    }
    return deststr;
}

qbs *qbs_add(qbs *str1, qbs *str2) {
    qbs *tqbs;
    if (!str2->len)
        return str1; // pass on
    if (!str1->len)
        return str2; // pass on
    // may be possible to acquire str1 or str2's space but...
    // 1. check if dest has enough space (because its data is already in the correct place)
    // 2. check if source has enough space
    // 3. give up
    // nb. they would also have to be a tmp, var. len str in ext memory!
    // brute force method...
    tqbs = qbs_new(str1->len + str2->len, 1);
    memcpy(tqbs->chr, str1->chr, str1->len);
    memcpy(tqbs->chr + str1->len, str2->chr, str2->len);

    // exit(qbs_sp);
    if (str1->tmp)
        qbs_free(str1);
    if (str2->tmp)
        qbs_free(str2);
    return tqbs;
}

qbs *qbs_ucase(qbs *str) {
    if (!str->len)
        return str;
    qbs *tqbs = NULL;
    if (str->tmp && !str->fixed && !str->readonly && !str->in_cmem) {
        tqbs = str;
    } else {
        tqbs = qbs_new(str->len, 1);
        memcpy(tqbs->chr, str->chr, str->len);
    }
    unsigned char *c = tqbs->chr;
    for (int32 i = 0; i < str->len; i++) {
        if ((*c >= 'a') && (*c <= 'z'))
            *c = *c & 223;
        c++;
    }
    if (tqbs != str && str->tmp)
        qbs_free(str);
    return tqbs;
}

qbs *qbs_lcase(qbs *str) {
    if (!str->len)
        return str;
    qbs *tqbs = NULL;
    if (str->tmp && !str->fixed && !str->readonly && !str->in_cmem) {
        tqbs = str;
    } else {
        tqbs = qbs_new(str->len, 1);
        memcpy(tqbs->chr, str->chr, str->len);
    }
    unsigned char *c = tqbs->chr;
    for (int32 i = 0; i < str->len; i++) {
        if ((*c >= 'A') && (*c <= 'Z'))
            *c = *c | 32;
        c++;
    }
    if (tqbs != str && str->tmp)
        qbs_free(str);
    return tqbs;
}

qbs *func_chr(int32 value) {
    qbs *tqbs;
    if ((value < 0) || (value > 255)) {
        tqbs = qbs_new(0, 1);
        error(5);
    } else {
        tqbs = qbs_new(1, 1);
        tqbs->chr[0] = value;
    }
    return tqbs;
}

qbs *func_varptr_helper(uint8 type, uint16 offset) {
    //*creates a 3 byte string using the values given
    qbs *tqbs;
    tqbs = qbs_new(3, 1);
    tqbs->chr[0] = type;
    tqbs->chr[1] = offset & 255;
    tqbs->chr[2] = offset >> 8;
    return tqbs;
}

qbs *qbs_left(qbs *str, int32 l) {
    if (l > str->len)
        l = str->len;
    if (l < 0)
        l = 0;
    if (l == str->len)
        return str; // pass on
    if (str->tmp) {
        if (!str->fixed) {
            if (!str->readonly) {
                if (!str->in_cmem) {
                    str->len = l;
                    return str;
                }
            }
        }
    }
    qbs *tqbs;
    tqbs = qbs_new(l, 1);
    if (l)
        memcpy(tqbs->chr, str->chr, l);
    if (str->tmp)
        qbs_free(str);
    return tqbs;
}

qbs *qbs_right(qbs *str, int32 l) {
    if (l > str->len)
        l = str->len;
    if (l < 0)
        l = 0;
    if (l == str->len)
        return str; // pass on
    if (str->tmp) {
        if (!str->fixed) {
            if (!str->readonly) {
                if (!str->in_cmem) {
                    str->chr = str->chr + (str->len - l);
                    str->len = l;
                    return str;
                }
            }
        }
    }
    qbs *tqbs;
    tqbs = qbs_new(l, 1);
    if (l)
        memcpy(tqbs->chr, str->chr + str->len - l, l);
    tqbs->len = l;
    if (str->tmp)
        qbs_free(str);
    return tqbs;
}
