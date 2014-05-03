/* Copyright (C) 2007-2010 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * \file
 *
 * \author Victor Julien <victor@inliniac.net>
 */

#ifndef __APP_LAYER_DETECT_PROTO_H__
#define __APP_LAYER_DETECT_PROTO_H__

#include "stream.h"
#include "detect-content.h"
#include "app-layer-parser.h"

/** \brief Signature for proto detection
 *  \todo we might just use SigMatch here
 */
typedef struct AlpProtoSignature_ {
    uint16_t ip_proto;                     /**< protocol (TCP/UDP) */
    uint16_t proto;                     /**< protocol */
    DetectContentData *co;              /**< content match that needs to match */
    struct AlpProtoSignature_ *next;    /**< next signature */
    struct AlpProtoSignature_ *map_next;    /**< next signature with same id */
} AlpProtoSignature;

#define ALP_DETECT_MAX 256

typedef struct AlpProtoDetectDirection_ {
    MpmCtx mpm_ctx;
    uint32_t id;
    uint16_t map[ALP_DETECT_MAX];   /**< a mapping between condition id's and
                                         protocol */
    uint16_t max_len;               /**< max length of all patterns, so we can
                                         limit the search */
    uint16_t min_len;               /**< min length of all patterns, so we can
                                         tell the stream engine to feed data
                                         to app layer as soon as it has min
                                         size data */
    uint32_t async_max;             /**< max bytes in this direction while 0 in
                                         the other, before we give up. */
} AlpProtoDetectDirection;

typedef struct AlpProtoDetectCtx_ {
    AlpProtoDetectDirection toserver;
    AlpProtoDetectDirection toclient;

    MpmPatternIdStore *mpm_pattern_id_store;    /** pattern id store */

    /** Mapping between pattern id and signature. As each signature has a
     *  unique pattern with a unique id, we can lookup the signature by
     *  the pattern id. */
    AlpProtoSignature **map;

    AlpProtoSignature *head;    // ������ÿ��Э��������롢Э��ŵȡ�
    AppLayerProbingParser *probing_parsers;
    uint16_t sigs;              /**< number of sigs */
} AlpProtoDetectCtx;

extern AlpProtoDetectCtx alp_proto_ctx;

#define FLOW_IS_PM_DONE(f, dir) (((dir) & STREAM_TOSERVER) ? ((f)->flags & FLOW_TS_PM_ALPROTO_DETECT_DONE) : ((f)->flags & FLOW_TC_PM_ALPROTO_DETECT_DONE))
#define FLOW_IS_PP_DONE(f, dir) (((dir) & STREAM_TOSERVER) ? ((f)->flags & FLOW_TS_PP_ALPROTO_DETECT_DONE) : ((f)->flags & FLOW_TC_PP_ALPROTO_DETECT_DONE))

#define FLOW_SET_PM_DONE(f, dir) (((dir) & STREAM_TOSERVER) ? ((f)->flags |= FLOW_TS_PM_ALPROTO_DETECT_DONE) : ((f)->flags |= FLOW_TC_PM_ALPROTO_DETECT_DONE))
#define FLOW_SET_PP_DONE(f, dir) (((dir) & STREAM_TOSERVER) ? ((f)->flags |= FLOW_TS_PP_ALPROTO_DETECT_DONE) : ((f)->flags |= FLOW_TC_PP_ALPROTO_DETECT_DONE))

#define FLOW_RESET_PM_DONE(f, dir) (((dir) & STREAM_TOSERVER) ? ((f)->flags &= ~FLOW_TS_PM_ALPROTO_DETECT_DONE) : ((f)->flags &= ~FLOW_TC_PM_ALPROTO_DETECT_DONE))
#define FLOW_RESET_PP_DONE(f, dir) (((dir) & STREAM_TOSERVER) ? ((f)->flags &= ~FLOW_TS_PP_ALPROTO_DETECT_DONE) : ((f)->flags &= ~FLOW_TC_PP_ALPROTO_DETECT_DONE))

void AlpProtoInit(AlpProtoDetectCtx *);
void *AppLayerDetectProtoThread(void *td);

void AppLayerDetectProtoThreadInit(void);

uint16_t AppLayerDetectGetProtoPMParser(AlpProtoDetectCtx *ctx,
                                        AlpProtoDetectThreadCtx *tctx,
                                        Flow *f,
                                        uint8_t *buf, uint16_t buflen,
                                        uint8_t flags, uint8_t ipproto,
                                        uint16_t *pm_results);
uint16_t AppLayerDetectGetProtoProbingParser(AlpProtoDetectCtx *, Flow *,
                                             uint8_t *, uint32_t,
                                             uint8_t, uint8_t);
uint16_t AppLayerDetectGetProto(AlpProtoDetectCtx *, AlpProtoDetectThreadCtx *,
                                Flow *, uint8_t *, uint32_t,
                                uint8_t, uint8_t);
void AlpProtoAddCI(AlpProtoDetectCtx *, char *, uint16_t, uint16_t, char *, uint16_t, uint16_t, uint8_t);
void AlpProtoAdd(AlpProtoDetectCtx *, char *, uint16_t, uint16_t, char *, uint16_t, uint16_t, uint8_t);

void AppLayerDetectProtoThreadSpawn(void);
void AlpDetectRegisterTests(void);

void AlpProtoFinalizeGlobal(AlpProtoDetectCtx *);
void AlpProtoFinalizeThread(AlpProtoDetectCtx *, AlpProtoDetectThreadCtx *);
void AlpProtoFinalize2Thread(AlpProtoDetectThreadCtx *);
void AlpProtoDeFinalize2Thread (AlpProtoDetectThreadCtx *);
void AlpProtoTestDestroy(AlpProtoDetectCtx *);
void AlpProtoDestroy(void);

#endif /* __APP_LAYER_DETECT_PROTO_H__ */

