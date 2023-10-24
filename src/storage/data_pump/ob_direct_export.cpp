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
#define USING_LOG_PREFIX STORAGE
#include "storage/data_pump/ob_direct_export.h"
#include "storage/data_pump/ob_local_scanner.h"

namespace oceanbase
{
namespace storage
{

int ObDirectExport::init(share::ObLSID ls_id, uint64_t tenant_id, uint64_t table_id,
                         ObTabletID tablet_id, ObString path)
{
    int ret = OB_SUCCESS;
    char* dump_dir_buf = nullptr;
    if (IS_INIT) {
        ret = OB_INIT_TWICE;
        LOG_WARN("init twice", K(ret));
    } else if (path.length() <= 0 || path.length() >= PATH_LEN) {
        ret = OB_INVALID_ARGUMENT;
        LOG_WARN("invalid path length", K(ret), K(path.length()));
    } else if (OB_ISNULL(dump_dir_buf = static_cast<char*>(allocator_.alloc(PATH_LEN)))) {
        ret = OB_ALLOCATE_MEMORY_FAILED;
        LOG_WARN("allocate dump dir buf failed", K(ret), K(PATH_LEN));
    } else {
        MEMSET(dump_dir_buf, 0, PATH_LEN);
        MEMCPY(dump_dir_buf, path.ptr(), path.length());
        dump_dir_.assign_buffer(dump_dir_buf, PATH_LEN);
        dump_dir_.set_length(path.length());
        ls_id_ = ls_id;
        tenant_id_ = tenant_id;
        table_id_ = table_id;
        tablet_id_ = tablet_id;
        is_inited_ = true;
    }
    return ret;
}

int ObDirectExport::dump_minor_sst()
{
    int ret = OB_SUCCESS;
    blocksstable::ObDatumRange range; // range should live longer than ObLocalScanner
    ObLocalScanner local_scanner;
    // ObExportSSTableWriter sst_writer;
    // ObDumpFileWriter file_writer;
    range.set_whole_range();
    if (IS_NOT_INIT) {
        ret = OB_NOT_INIT;
        LOG_WARN("direct export is not inited", K(ret));
    } else if (OB_FAIL(local_scanner.init(ls_id_, tenant_id_, table_id_, tablet_id_, range))) {
        LOG_WARN("inti local scanner failed", K(ret));
    } else {
        const blocksstable::ObDatumRow *row = nullptr;
        int row_count = 0;
        bool iter_end = false;
        // char datum_str_buf[4096];
        // MEMSET(datum_str_buf, 0, sizeof(char) * 4096);
        while (OB_SUCC(ret) && !iter_end) {
            if (OB_FAIL(local_scanner.get_next_row(row))) {
                if (OB_UNLIKELY(OB_ITER_END != ret)) {
                    LOG_WARN("get next row failed", K(ret));
                } else {
                    iter_end = true;
                    ret = OB_SUCCESS;
                }
            } else {
                // row->to_string(datum_str_buf, 4096);
                row_count++;
            }
        }
    }

    ObTabletHandle tablet_handle;

    return ret;
}
int ObDirectExport::dump_major_sst()
{
    int ret = OB_SUCCESS;
    if (IS_NOT_INIT) {
        ret = OB_NOT_INIT;
        LOG_WARN("direct export is not inited", K(ret));
    } else {

    }
    return ret;
}
int ObDirectExport::dump_autoinc_seq_value()
{
    int ret = OB_SUCCESS;
    if (IS_NOT_INIT) {

    }
    return ret;
}

int ObDirectExport::dump_sst(ObITable *table, ObTabletHandle &tablet_handle, bool is_minor)
{
    int ret = OB_SUCCESS;
    return ret;
}

}
}