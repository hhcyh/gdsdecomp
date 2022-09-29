#include "pck_dumper.h"
#include "compat/resource_loader_compat.h"
#include "gdre_settings.h"

#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_encrypted.h"
#include "core/os/os.h"
#include "core/variant/variant_parser.h"
#include "core/version_generated.gen.h"
#include "modules/regex/regex.h"

bool PckDumper::_pck_file_check_md5(Ref<PackedFileInfo> &file) {
	// Loading an encrypted file automatically checks the md5
	if (file->is_encrypted()) {
		return true;
	}
	auto hash = FileAccess::get_md5(file->get_path());
	auto p_md5 = String::md5(file->get_md5().ptr());
	return hash == p_md5;
}

Error PckDumper::check_md5_all_files() {
	String ext = GDRESettings::get_singleton()->get_pack_path().get_extension();
	if (ext != "pck" || ext != "exe") {
		print_line("Not a pack file, skipping MD5 check...");
		return OK;
	}
	Error err = OK;
	auto files = GDRESettings::get_singleton()->get_file_info_list();
	for (int i = 0; i < files.size(); i++) {
		files.write[i]->set_md5_match(_pck_file_check_md5(files.write[i]));
		if (files[i]->md5_passed) {
			print_line("Verified " + files[i]->path);
		} else {
			print_error("Checksum failed for " + files[i]->path);
			err = ERR_BUG;
		}
	}
	return err;
}

Error PckDumper::pck_dump_to_dir(const String &dir, const Vector<String> &files_to_extract = Vector<String>()) {
	ERR_FAIL_COND_V_MSG(!GDRESettings::get_singleton()->is_pack_loaded(), ERR_DOES_NOT_EXIST,
			"Pack not loaded!");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	auto files = GDRESettings::get_singleton()->get_file_info_list();
	Vector<uint8_t> key = GDRESettings::get_singleton()->get_encryption_key();
	if (da.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}
	String failed_files;
	Error err;
	for (int i = 0; i < files.size(); i++) {
		if (files_to_extract.size() && !files_to_extract.has(files.get(i)->get_path())) {
			continue;
		}
		Ref<FileAccess> pck_f = FileAccess::open(files.get(i)->get_path(), FileAccess::READ, &err);
		if (pck_f.is_null()) {
			failed_files += files.get(i)->get_path() + " (FileAccess error)\n";
			continue;
		}
		String target_name = dir.path_join(files.get(i)->get_path().replace("res://", ""));
		da->make_dir_recursive(target_name.get_base_dir());
		Ref<FileAccess> fa = FileAccess::open(target_name, FileAccess::WRITE);
		if (fa.is_null()) {
			failed_files += files.get(i)->get_path() + " (FileWrite error)\n";
			continue;
		}

		int64_t rq_size = files.get(i)->get_size();
		uint8_t buf[16384];
		while (rq_size > 0) {
			int got = pck_f->get_buffer(buf, MIN(16384, rq_size));
			fa->store_buffer(buf, got);
			rq_size -= 16384;
		}
		fa->flush();
		print_line("Extracted " + target_name);
	}

	if (failed_files.length() > 0) {
		print_error("At least one error was detected while extracting pack!\n" + failed_files);
		//show_warning(failed_files, RTR("Read PCK"), RTR("At least one error was detected!"));
	} else {
		print_line("No errors detected!");
		//show_warning(RTR("No errors detected."), RTR("Read PCK"), RTR("The operation completed successfully!"));
	}
	return OK;
}

void PckDumper::_bind_methods() {
	ClassDB::bind_method(D_METHOD("check_md5_all_files"), &PckDumper::check_md5_all_files);
	ClassDB::bind_method(D_METHOD("pck_dump_to_dir", "dir", "files_to_extract"), &PckDumper::pck_dump_to_dir, DEFVAL(Vector<String>()));
	//ClassDB::bind_method(D_METHOD("get_dumped_files"), &PckDumper::get_dumped_files);
}
