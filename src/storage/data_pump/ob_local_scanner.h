/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PubL v2. You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PubL v2 for more details.
 */

#ifndef OCEANBASE_STORAGE_OB_LOCAL_SCANNER_H
#define OCEANBASE_STORAGE_OB_LOCAL_SCANNER_H

#include "common/ob_tablet_id.h"
#include "share/ob_ls_id.h"
#include "storage/blocksstable/ob_datum_row.h"
#include "share/schema/ob_table_schema.h"
#include "share/schema/ob_table_param.h"
#include "share/schema/ob_column_schema.h"
#include "storage/access/ob_table_read_info.h"
#include "storage/access/ob_table_access_param.h"
#include "common/ob_common_types.h"
#include "storage/meta_mem/ob_tablet_handle.h"
#include "storage/ob_i_store.h"
#include "storage/access/ob_table_access_context.h"
#include "storage/access/ob_multiple_scan_merge.h"

namespace oceanbase
{
namespace storage
{

// With reference of ObLocalScan
class ObLocalScanner 
{
public:
    ObLocalScanner() : is_inited_(false) {}
    ~ObLocalScanner() {}
    int init(share::ObLSID ls_id, uint64_t tenant_id, uint64_t table_id, ObTabletID tablet_id, blocksstable::ObDatumRange& range);
    int get_next_row(const blocksstable::ObDatumRow *&row);
private:
    int construct_column_schema(const share::schema::ObTableSchema &table_schema);
    int construct_access_param(const ObTableSchema &table_schema, const ObTabletID &tablet_id);
    int construct_range_ctx(ObQueryFlag &query_flag, const share::ObLSID &ls_id);
    int construct_multiple_scan_merge(ObTabletTableIterator &tablet_iter, blocksstable::ObDatumRange &range);

    bool is_inited_;
    share::ObLSID ls_id_;
    uint64_t table_id_;
    ObTabletID tablet_id_;
    uint64_t tenant_id_;
    // construct_column_schema
    common::ObArray<share::schema::ObColumnParam *> col_params_;
    common::ObArenaAllocator allocator_;
    // construct_access_param
    ObTableReadInfo read_info_;              // the ColumnParams to read (sstables and memtables are different)
    ObArray<int32_t> output_projector_;
    ObTableAccessParam access_param_;
    // construct_range_ctx
    ObStoreCtx ctx_;
    ObTableAccessContext access_ctx_;
    // construct_multiple_scan_merge
    ObMultipleScanMerge *scan_merge_;
    ObGetTableParam get_table_param_;
};

}
}

#endif