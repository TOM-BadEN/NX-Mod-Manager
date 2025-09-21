/*
 * Copyright (c) Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <haze.hpp>
#include <haze/ptp_data_builder.hpp>
#include <haze/ptp_data_parser.hpp>
#include <haze/ptp_responder_types.hpp>
#include <ctime>

namespace haze {

    namespace {

        PtpBuffers *GetBuffers() {
            static constinit PtpBuffers buffers = {};
            return std::addressof(buffers);
        }

        const char* FixName(const char* name) {
            if (name[0] == '/' && name[1] == '/') {
                return name + 1;
            }
            return name;
        }

    }

    Result PtpResponder::Initialize(EventReactor *reactor, PtpObjectHeap *object_heap, const FsEntries& entries, u16 vid, u16 pid) {
        log_write("Initializing PTP responder...\n");
        m_object_heap = object_heap;
        m_buffers = GetBuffers();
        m_fs_entries.clear();

        u32 storage_id = StorageId_DefaultStorage;

        for (const auto& e : entries) {
            m_fs_entries.emplace_back(storage_id, FileSystemProxy{reactor, e});
            storage_id--;
        }

        /* Configure fs proxy. */
        log_write("Configured %zu filesystem entries\n", m_fs_entries.size());
        R_RETURN(m_usb_server.Initialize(std::addressof(MtpInterfaceInfo), vid, pid, reactor));
    }

    void PtpResponder::Finalize() {
        m_usb_server.Finalize();
    }

    Result PtpResponder::LoopProcess() {
        while (true) {
            /* Try to handle a request. */
            R_TRY_CATCH(this->HandleRequest()) {
                R_CATCH(haze::ResultStopRequested, haze::ResultFocusLost) {
                    /* If we encountered a stop condition, we're done.*/
                    R_THROW(R_CURRENT_RESULT);
                }
                R_CATCH_ALL() {
                    /* On other failures, try to handle another request. */
                    continue;
                }
            } R_END_TRY_CATCH;

            /* Otherwise, handle the next request. */
            /* ... */
        }
    }

    Result PtpResponder::HandleRequest() {
        ON_RESULT_FAILURE {
            log_write("Request handling failed.\n");
            /* For general failure modes, the failure is unrecoverable. Close the session. */
            this->ForceCloseSession();
        };

        R_TRY_CATCH(this->HandleRequestImpl()) {
            R_CATCH(haze::ResultUnknownRequestType) {
                log_write("Error: Unknown request type: 0x%04x\n", m_request_header.type);
                R_TRY(this->WriteResponse(PtpResponseCode_GeneralError));
            }
            R_CATCH(haze::ResultSessionNotOpen) {
                log_write("Error: Session not open.\n");
                R_TRY(this->WriteResponse(PtpResponseCode_SessionNotOpen));
            }
            R_CATCH(haze::ResultOperationNotSupported) {
                log_write("Error: Operation not supported: 0x%04x\n", m_request_header.code);
                R_TRY(this->WriteResponse(PtpResponseCode_OperationNotSupported));
            }
            R_CATCH(haze::ResultInvalidStorageId) {
                log_write("Error: Invalid storage ID.\n");
                R_TRY(this->WriteResponse(PtpResponseCode_InvalidStorageId));
            }
            R_CATCH(haze::ResultInvalidObjectId) {
                log_write("Error: Invalid object ID.\n");
                R_TRY(this->WriteResponse(PtpResponseCode_InvalidObjectHandle));
            }
            R_CATCH(haze::ResultUnknownPropertyCode) {
                log_write("Error: Unknown property code.\n");
                R_TRY(this->WriteResponse(PtpResponseCode_MtpObjectPropNotSupported));
            }
            R_CATCH(haze::ResultInvalidPropertyValue) {
                log_write("Error: Invalid property value.\n");
                R_TRY(this->WriteResponse(PtpResponseCode_MtpInvalidObjectPropValue));
            }
            R_CATCH(haze::ResultGroupSpecified) {
                log_write("Error: Specification by group unsupported.\n");
                R_TRY(this->WriteResponse(PtpResponseCode_MtpSpecificationByGroupUnsupported));
            }
            R_CATCH(haze::ResultDepthSpecified) {
                log_write("Error: Specification by depth unsupported.\n");
                R_TRY(this->WriteResponse(PtpResponseCode_MtpSpecificationByDepthUnsupported));
            }
            R_CATCH(haze::ResultInvalidArgument) {
                log_write("Error: Invalid argument.\n");
                R_TRY(this->WriteResponse(PtpResponseCode_GeneralError));
            }
            R_CATCH_MODULE(fs) {
                /* Errors from fs are typically recoverable. */
                log_write("Filesystem error: 0x%08x\n", R_CURRENT_RESULT.GetValue());
                R_TRY(this->WriteResponse(PtpResponseCode_GeneralError));
            }
        } R_END_TRY_CATCH;

        R_SUCCEED();
    }

    Result PtpResponder::HandleRequestImpl() {
        PtpDataParser dp(m_buffers->usb_bulk_read_buffer, std::addressof(m_usb_server));
        R_TRY(dp.Read(std::addressof(m_request_header)));

        switch (m_request_header.type) {
            case PtpUsbBulkContainerType_Command: R_RETURN(this->HandleCommandRequest(dp));
            default:
                log_write("Error: Unknown request type: 0x%04x\n", m_request_header.type);
                R_THROW(haze::ResultUnknownRequestType());
        }
    }

    Result PtpResponder::HandleCommandRequest(PtpDataParser &dp) {
        if (!m_session_open && m_request_header.code != PtpOperationCode_OpenSession && m_request_header.code != PtpOperationCode_GetDeviceInfo)  {
            R_THROW(haze::ResultSessionNotOpen());
        }

        log_write("Handling request: 0x%04x\n", m_request_header.code);
        switch (m_request_header.code) {
            case PtpOperationCode_GetDeviceInfo:              R_RETURN(this->GetDeviceInfo(dp));           break;
            case PtpOperationCode_OpenSession:                R_RETURN(this->OpenSession(dp));             break;
            case PtpOperationCode_CloseSession:               R_RETURN(this->CloseSession(dp));            break;
            case PtpOperationCode_GetStorageIds:              R_RETURN(this->GetStorageIds(dp));           break;
            case PtpOperationCode_GetStorageInfo:             R_RETURN(this->GetStorageInfo(dp));          break;
            case PtpOperationCode_GetObjectHandles:           R_RETURN(this->GetObjectHandles(dp));        break;
            case PtpOperationCode_GetObjectInfo:              R_RETURN(this->GetObjectInfo(dp));           break;
            case PtpOperationCode_GetObject:                  R_RETURN(this->GetObject(dp));               break;
            case PtpOperationCode_SendObjectInfo:             R_RETURN(this->SendObjectInfo(dp));          break;
            case PtpOperationCode_SendObject:                 R_RETURN(this->SendObject(dp));              break;
            case PtpOperationCode_DeleteObject:               R_RETURN(this->DeleteObject(dp));            break;
            case PtpOperationCode_MtpGetObjectPropsSupported: R_RETURN(this->GetObjectPropsSupported(dp)); break;
            case PtpOperationCode_MtpGetObjectPropDesc:       R_RETURN(this->GetObjectPropDesc(dp));       break;
            case PtpOperationCode_MtpGetObjectPropValue:      R_RETURN(this->GetObjectPropValue(dp));      break;
            case PtpOperationCode_MtpSetObjectPropValue:      R_RETURN(this->SetObjectPropValue(dp));      break;
            case PtpOperationCode_MtpGetObjPropList:          R_RETURN(this->GetObjectPropList(dp));       break;
            case PtpOperationCode_MtpSendObjectPropList:      R_RETURN(this->SendObjectPropList(dp));      break;
            default:
            {
                log_write("Error: Operation not supported: 0x%04x\n", m_request_header.code);
                R_THROW(haze::ResultOperationNotSupported());
            }
        }
    }

    void PtpResponder::ForceCloseSession() {
        log_write("Force closing session.\n");
        if (m_session_open) {
            m_session_open = false;
            m_object_database.Finalize();
        }
    }

    Result PtpResponder::WriteResponse(PtpResponseCode code, const void* data, size_t size) {
        PtpDataBuilder db(m_buffers->usb_bulk_write_buffer, std::addressof(m_usb_server));
        R_TRY(db.AddResponseHeader(m_request_header, code, size));
        R_TRY(db.AddBuffer(reinterpret_cast<const u8*>(data), size));
        R_RETURN(db.Commit());
    }

    Result PtpResponder::WriteResponse(PtpResponseCode code) {
        PtpDataBuilder db(m_buffers->usb_bulk_write_buffer, std::addressof(m_usb_server));
        R_TRY(db.AddResponseHeader(m_request_header, code, 0));
        R_RETURN(db.Commit());
    }

    #if 0
    void PtpResponder::WriteCallbackSession(CallbackType type) {}
    void PtpResponder::WriteCallbackFile(CallbackType type, const char* name) {}
    void PtpResponder::WriteCallbackRename(CallbackType type, const char* name, const char* newname) {}
    void PtpResponder::WriteCallbackProgress(CallbackType type, s64 offset, s64 size) {}
    #else
    void PtpResponder::WriteCallbackSession(CallbackType type) {
        if (!m_callback) {
            return;
        }
        CallbackData data{type};
        m_callback(&data);
    }

    void PtpResponder::WriteCallbackFile(CallbackType type, const char* name) {
        if (!m_callback) {
            return;
        }
        CallbackData data{type};
        std::strncpy(data.file.filename, FixName(name), sizeof(data.file.filename)-1);
        m_callback(&data);
    }

    void PtpResponder::WriteCallbackRename(CallbackType type, const char* name, const char* newname) {
        if (!m_callback) {
            return;
        }
        CallbackData data{type};
        std::strncpy(data.rename.filename, FixName(name), sizeof(data.rename.filename)-1);
        std::strncpy(data.rename.newname, FixName(newname), sizeof(data.rename.newname)-1);
        m_callback(&data);
    }

    void PtpResponder::WriteCallbackProgress(CallbackType type, s64 offset, s64 size) {
        if (!m_callback) {
            return;
        }
        CallbackData data{type};
        data.progress.offset = offset;
        data.progress.size = size;
        m_callback(&data);
    }
    #endif

    const char* PtpResponder::BuildTimeStamp(char* out, u64 timestamp) {
        if (!timestamp) {
            return "";
        }

        std::tm tm;
        const auto time = (std::time_t)timestamp;
        if (!localtime_r(&time, &tm)) {
            return "";
        }

        // todo: make sure this is the correct format.
        // also, make sure the timezone is correct.
        std::strftime(out, PtpStringMaxLength, "%Y%m%dT%H%M%S", &tm);
        return out;
    }
}
