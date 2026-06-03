from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.files import copy
from conan.tools.layout import basic_layout
import os


class PolyLiteConan(ConanFile):
    name = "polylite"
    description = (
        "Header-only C++23 multiprotocol library for CBOR, JSON, TOML and YAML."
    )
    license = "MIT"
    url = "https://github.com/kristianivarsson/polylite"
    homepage = "https://github.com/kristianivarsson/polylite"
    topics = ("json", "yaml", "toml", "cbor", "header-only", "cpp23", "parser")
    settings = "os", "arch", "compiler", "build_type"
    package_type = "header-library"
    no_copy_source = True

    def export_sources(self):
        repo_root = os.path.join(self.recipe_folder, "..", "..")
        copy(
            self,
            "*.hpp",
            src=os.path.join(repo_root, "include"),
            dst=os.path.join(self.export_sources_folder, "include"),
        )
        copy(self, "LICENSE", src=repo_root, dst=self.export_sources_folder)
        copy(self, "README.md", src=repo_root, dst=self.export_sources_folder)

    def layout(self):
        basic_layout(self, src_folder=".")

    def validate(self):
        check_min_cppstd(self, 23)

    def package(self):
        copy(
            self,
            "*.hpp",
            src=os.path.join(self.source_folder, "include"),
            dst=os.path.join(self.package_folder, "include"),
        )
        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )

    def package_id(self):
        self.info.clear()

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.set_property("cmake_file_name", "polylite")
        self.cpp_info.set_property("cmake_target_name", "polylite::polylite")
