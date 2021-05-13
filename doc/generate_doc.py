#!/usr/bin/env python3

'''
This tool is for internal use only.
There is no guarantee of correct work, and there is no support.

Please take care when using it, because this script manipulates nrf git repository.
It is a good practice to have clean repositories, so commit all changes before use.

required packages:
- GitPython==3.1.16

How to use it:
The script should be located in `find-my/doc` directory, and it should be executed there (other place of execution, may cause broken paths)
The script does not take any command line arguments.
The script will generate html version of the Find My documentation, and it will be placed in find-my/doc/html directory.
If the html directory does not exist, it will be created.
'''

import os
import shutil
from git import Repo
import subprocess


PATCH_FORK = 'https://github.com/kapi-no/fw-nrfconnect-nrf.git'
PATCH_FORK_BRANCH_NAME = 'find_my_doc_fork'

BUILD_DIR = "../../nrf/doc/build"
FMN_BUILD_DIR = BUILD_DIR + "/html/find-my"
TARGET_DIR = "_build"
TARGET_HTML_DIR = TARGET_DIR + '/html'
TARGET_ZIP_NAME = "latest"

def findRemoteWithDocFix(repo):
    '''
    Looks into nrf repository in order to find
    if it already have remote with fork that contains fix
    '''
    for git_remote in repo.remotes:
        if git_remote.url == PATCH_FORK:
            return git_remote
    return None


def pull_nrf_patch():
    '''
    This function checks out the nrf repository to state that contain fix
    for find-my documentation build
    '''
    nrf_repo = Repo("../../nrf")
    current_commit = nrf_repo.head.object.hexsha
    fork_remote = findRemoteWithDocFix(nrf_repo)
    if fork_remote is None:
        fork_remote = nrf_repo.create_remote(PATCH_FORK_BRANCH_NAME, PATCH_FORK)
        fork_remote.fetch()

    nrf_repo.git.checkout(PATCH_FORK_BRANCH_NAME)
    return (nrf_repo, current_commit, fork_remote)


def restore_nrf_repo(repo, commit, fork_to_clean):
    '''
    After the build, the nrf repository has to be restored to original state
    '''
    repo.git.checkout(commit)
    repo.delete_remote(fork_to_clean)


def build_find_my_doc():
    '''
    This function is responsible for building the documentation,
    and cleaning the build directory in nrf
    The generated html output is placed in find-my/doc/html directory
    '''
    if os.path.isdir(BUILD_DIR):
        shutil.rmtree(BUILD_DIR)
    os.mkdir(BUILD_DIR)

    cmake_result = subprocess.run(['cmake', '-GNinja', '../'], cwd=BUILD_DIR)
    if (cmake_result.returncode != 0):
        exit(-1)
    build_result = subprocess.run(['ninja', 'find-my-html'], cwd=BUILD_DIR)
    if (build_result.returncode != 0):
        exit(-2)


def prepare_doc_artifacts():
    '''
    After the build process, files have to be moved to the Find My repo
    and the zip artifact has to be created.
    '''
    if os.path.isdir(TARGET_DIR):
        shutil.rmtree(TARGET_DIR)
    shutil.copytree(FMN_BUILD_DIR, TARGET_HTML_DIR)
    shutil.make_archive(os.path.join(".", TARGET_DIR, TARGET_ZIP_NAME),
                        format='zip', root_dir=FMN_BUILD_DIR)

def main():
    '''
    The main function of the application is to generate up to date
    Find My documentation.
    NCS has to be patched with fix from remote fork.
    Build has to be executed.
    Generated html have to be fixed.
    '''
    nrf_repo, current_commit, fork_remote = pull_nrf_patch()
    build_find_my_doc()
    prepare_doc_artifacts()
    restore_nrf_repo(nrf_repo, current_commit, fork_remote)


if __name__ == "__main__":
    main()
