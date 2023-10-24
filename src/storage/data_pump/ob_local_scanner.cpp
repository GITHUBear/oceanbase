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
#include "storage/data_pump/ob_local_scanner.h"
#include "storage/tx_storage/ob_ls_handle.h"
#include "storage/tx_storage/ob_ls_service.h"
#include "share/schema/ob_schema_getter_guard.h"

namespace oceanbase
{
namespace storage
{

int ObLocalScanner::init(share::ObLSID ls_id, uint64_t tenant_id, uint64_t table_id, ObTabletID tablet_id, blocksstable::ObDatumRange &range)
{
    int ret = OB_SUCCESS;
    ls_id_ = ls_id;
    tenant_id_ = tenant_id;
    table_id_ = table_id;
    tablet_id_ = tablet_id;

    ObQueryFlag query_flag(
        ObQueryFlag::Forward, true, /*is daily merge scan*/  // TODO: false
        true,                       /*is read multiple macro block*/
        false,                      /*sys task scan, read one macro block in single io*/
        true,                       /*is full row scan*/
        false, false);
    
    ObLSHandle ls_handle;
    ObTabletTableIterator iterator;  // iter on mem/mini/minor/major tables in Tablet
    if (IS_INIT) {
        ret = OB_INIT_TWICE;
        LOG_WARN("init twice", K(ret), KP(this));
    } else if (OB_FAIL(MTL(ObLSService *)->get_ls(ls_id_, ls_handle, ObLSGetMod::DDL_MOD))) {
        LOG_WARN("failed to get log stream", K(ret));
    } else if (OB_UNLIKELY(nullptr == ls_handle.get_ls())) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("ls is null", K(ret), K(ls_handle));
    } else if (OB_FAIL(ls_handle.get_ls()->get_tablet_svr()->get_read_tables(
                    tablet_id_, INT64_MAX, iterator, false))) {
        if (OB_REPLICA_NOT_READABLE == ret) {
            ret = OB_EAGAIN;
        } else {
            LOG_WARN("snapshot version has been discarded", K(ret));
        }
    } else {
        ObSchemaGetterGuard schema_guard;
        const ObTableSchema *table_schema = nullptr;
        if (OB_FAIL(ObMultiVersionSchemaService::get_instance().get_tenant_schema_guard(
                        tenant_id_, schema_guard))) {
            LOG_WARN("failed to get tenant schema guard", K(ret));
        } else if (OB_FAIL(schema_guard.get_table_schema(tenant_id_, table_id_,
                                                         table_schema))) {
            LOG_WARN("failed to get data table schema", K(ret));
        } else if (OB_ISNULL(table_schema)) {
            ret = OB_TABLE_NOT_EXIST;
            LOG_WARN("table schema not exist", K(ret));
        } else if (OB_FAIL(construct_column_schema(*table_schema))) {
            LOG_WARN("failed to construct column schema", K(ret));
        } else if (OB_FAIL(construct_access_param(*table_schema, tablet_id))) {
            LOG_WARN("failed to construct access param", K(ret));
        } else if (OB_FAIL(construct_range_ctx(query_flag, ls_id_))) {
            LOG_WARN("failed to construct range ctx", K(ret), K(query_flag));
        } else if (OB_FAIL(construct_multiple_scan_merge(iterator, range))) {
            LOG_WARN("failed to construct multiple scan merge", K(ret), K(iterator));
        } else {
            is_inited_ = true;
        }
    }
    return ret;
}

int ObLocalScanner::get_next_row(const blocksstable::ObDatumRow *&row)
{
    int ret = OB_SUCCESS;
    ObDatumRow *tmp_row = nullptr;
    if (!IS_INIT) {
        LOG_WARN("not inited", K(ret));
        ret = OB_NOT_INIT;
    } else if (OB_FAIL(scan_merge_->get_next_row(tmp_row))) {
        if (OB_UNLIKELY(OB_ITER_END != ret)) {
            LOG_WARN("failed to get next row", K(ret));
        }
    } else if (OB_ISNULL(tmp_row) || !tmp_row->is_valid()) {
        ret = OB_INVALID_ARGUMENT;
        LOG_WARN("invalid arguments", K(ret), KPC(tmp_row));
    } else {
        row = tmp_row;
    }
    return ret;
}

int ObLocalScanner::construct_column_schema(const share::schema::ObTableSchema &table_schema)
{
    int ret = OB_SUCCESS;
    col_params_.reset();
    ObArray<ObColDesc> col_ids;
    if (OB_FAIL(table_schema.get_multi_version_column_descs(col_ids))) {
        LOG_WARN("failed to get column id", K(ret));
    } else {
        ObColumnParam *tmp_col_param = nullptr;
        void* col_param_buf = nullptr;
        //  rowkey columns | extra rowkey columns | common columns 
        for (int64_t i = 0; OB_SUCC(ret) && i < col_ids.count(); ++i) {
            tmp_col_param = nullptr;
            col_param_buf = nullptr;

            if (i < table_schema.get_rowkey_column_num() || // rowkeys columns
                i > table_schema.get_rowkey_column_num() + ObMultiVersionRowkeyHelpper::get_extra_rowkey_col_cnt() // common columns
            ) {
                const ObColumnSchemaV2 *col = table_schema.get_column_schema(col_ids.at(i).col_id_);
                if (OB_ISNULL(col)) {
                    ret = OB_ERR_UNEXPECTED;
                    LOG_WARN("failed to get column schema", K(ret), K(col_ids.at(i).col_id_));
                } else if (OB_ISNULL(col_param_buf = allocator_.alloc(sizeof(ObColumnParam))) ||
                           OB_ISNULL(tmp_col_param = new (col_param_buf)ObColumnParam(allocator_))) {
                    ret = OB_ALLOCATE_MEMORY_FAILED;
                    LOG_WARN("failed to alloc memory", K(ret));
                } else if (OB_FAIL(ObTableParam::convert_column_schema_to_param(*col, *tmp_col_param))) {
                    LOG_WARN("convert column_schema to ColumnParam failed", K(ret));
                } else if (OB_FAIL(col_params_.push_back(tmp_col_param))) {
                    LOG_WARN("insert ColumnParam failed", K(ret));
                }
            } else { // extra rowkey columns
                // create ColumnParam manually
                if (OB_ISNULL(col_param_buf = allocator_.alloc(sizeof(ObColumnParam))) ||
                    OB_ISNULL(tmp_col_param = new (col_param_buf)ObColumnParam(allocator_))) {
                    ret = OB_ALLOCATE_MEMORY_FAILED;
                    LOG_WARN("failed to alloc memory", K(ret));
                } else {
                    tmp_col_param->set_column_id(col_ids[i].col_id_);
                    tmp_col_param->set_meta_type(col_ids[i].col_type_);
                    tmp_col_param->set_column_order(col_ids[i].col_order_);
                    tmp_col_param->set_nullable_for_write(true);
                    tmp_col_param->set_nullable_for_read(true);
                    ObObj null_obj;
                    null_obj.set_null();
                    tmp_col_param->set_orig_default_value(null_obj);
                    tmp_col_param->set_cur_default_value(null_obj);

                    if (OB_FAIL(col_params_.push_back(tmp_col_param))) {
                        LOG_WARN("insert ColumnParam failed", K(ret));
                    }
                }
            }
            if (OB_FAIL(ret) && OB_NOT_NULL(tmp_col_param)) {
                // clean
                tmp_col_param->~ObColumnParam();
                allocator_.free(tmp_col_param);
            }
        }
    }

    if (OB_FAIL(ret)) {
        // clean
        for (int64_t i = 0; i < col_params_.count(); i++) {
            ObColumnParam *&tmp_col_param = col_params_.at(i);
            if (OB_NOT_NULL(tmp_col_param)) {
                tmp_col_param->~ObColumnParam();
                allocator_.free(tmp_col_param);
                tmp_col_param = nullptr;
            }
        }
    }
    return ret;
}

int ObLocalScanner::construct_access_param(const ObTableSchema &table_schema, const ObTabletID &tablet_id)
{
    int ret = OB_SUCCESS;
    read_info_.reset();
    output_projector_.reset();
    ObArray<ObColDesc> col_ids;
    bool is_oracle_mode = false;
    if (OB_FAIL(table_schema.get_multi_version_column_descs(col_ids))) {
        LOG_WARN("failed to get column id", K(ret));
    } else {
        for (int64_t i = 0; OB_SUCC(ret) && i < col_ids.count(); i++) {
            if (OB_FAIL(output_projector_.push_back(i))) {
                LOG_WARN("failed to push to array", K(ret), K(i));
            }
        }
    }

    if (OB_FAIL(ret)) {
    } else if (OB_FAIL(table_schema.check_if_oracle_compat_mode(is_oracle_mode))) {
        LOG_WARN("check if oracle compat mode failed", K(ret));
    } else if (OB_FAIL(read_info_.init(allocator_, table_schema.get_column_count(),
                                       table_schema.get_rowkey_column_num(), is_oracle_mode,
                                       col_ids, &output_projector_, &col_params_, true))) {
        LOG_WARN("failed to init read info", K(ret));
    } else {
        // set acess_param
        access_param_.iter_param_.tablet_id_ = tablet_id;
        access_param_.iter_param_.table_id_ = table_schema.get_table_id();
        access_param_.iter_param_.out_cols_project_ = &output_projector_;
        access_param_.iter_param_.read_info_ = &read_info_;
        if (OB_FAIL(access_param_.iter_param_.refresh_lob_column_out_status())) {
            LOG_WARN("Failed to refresh lob column", K(ret), K(access_param_.iter_param_));
        } else {
            access_param_.is_inited_ = true;
        }
    }
    return ret;
}

int ObLocalScanner::construct_range_ctx(ObQueryFlag &query_flag, const share::ObLSID &ls_id)
{
    int ret = OB_SUCCESS;
    common::ObVersionRange trans_version_range;
    trans_version_range.snapshot_version_ = INT64_MAX;
    trans_version_range.multi_version_start_ = INT64_MAX;
    trans_version_range.base_version_ = 0;
    SCN tmp_scn;
    if (OB_FAIL(tmp_scn.convert_for_tx(INT64_MAX))) {
        LOG_WARN("convert failed", K(ret), K(ls_id));
    } else if (OB_FAIL(ctx_.init_for_read(ls_id, INT64_MAX, -1, tmp_scn))) {
        LOG_WARN("failed to init store ctx", K(ret), K(ls_id));
    } else if (FALSE_IT(ctx_.mvcc_acc_ctx_.tx_desc_ = nullptr)) {
    } else if (OB_FAIL(access_ctx_.init(query_flag, ctx_, allocator_, allocator_,
                                        trans_version_range))) {
        LOG_WARN("failed to init accesss ctx", K(ret));
    }
    return ret;
}

int ObLocalScanner::construct_multiple_scan_merge(ObTabletTableIterator &tablet_iter, blocksstable::ObDatumRange &range)
{
    int ret = OB_SUCCESS;
    void *scan_merge_buf = nullptr;
    // Use Sample feature to scan rows in specified scope.
    get_table_param_.tablet_iter_ = tablet_iter;
    get_table_param_.sample_info_.scope_ = SampleInfo::SampleScope::SAMPLE_BASE_DATA;  //
    get_table_param_.sample_info_.method_ = SampleInfo::SampleMethod::ROW_SAMPLE;

    if (OB_ISNULL(scan_merge_buf = allocator_.alloc(sizeof(ObMultipleScanMerge))) ||
        OB_ISNULL(scan_merge_ = new (scan_merge_buf)ObMultipleScanMerge())) {
        ret = OB_ALLOCATE_MEMORY_FAILED;
        LOG_WARN("failed to alloc memory for ObMultipleScanMerge", K(ret));
    } else if (OB_FAIL(scan_merge_->init(access_param_, access_ctx_, get_table_param_))) {
        LOG_WARN("failed to init scan merge", K(ret), K(access_param_), K(access_ctx_));
    } else if (OB_FAIL(scan_merge_->open(range))) {
        LOG_WARN("failed to open scan merge", K(ret));
    } else {
        scan_merge_->disable_padding();
        scan_merge_->disable_fill_virtual_column();
        scan_merge_->disable_fill_default();
        scan_merge_->set_iter_del_row(true);
    }

    if (OB_FAIL(ret) && OB_NOT_NULL(scan_merge_)) {
        scan_merge_->~ObMultipleScanMerge();
        allocator_.free(scan_merge_);
        scan_merge_ = nullptr;
    }
    return ret;
}

}
}