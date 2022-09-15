#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import os
import re
import sys
import stat
import urllib.parse
import subprocess
import tempfile
import shutil

branch_name = 'temp-compose-superbinary-now'
commit_message = 'TEMP: Adding files for SuperBinary composing on github.'

forbidden_repos = [
    r'github\.com/nrfconnect/.*'
]

def join_args(args):
    args = args[:]
    for i in range(0, len(args)):
        if args[i].find(' ') >= 0:
            args[i] = '"' + args[i] +'"'
    return ' '.join(args)

def exec(args, cwd = None, capture = False):
    print('Executing subprocess:')
    print(join_args(args))
    if capture:
        proc = subprocess.run(args, shell=False, check=False, cwd=cwd, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, encoding='utf-8')
        print(proc.stdout)
        proc.check_returncode()
        return proc.stdout
    else:
        subprocess.run(args, shell=False, check=True, cwd=cwd)

def get_content():
    global script_dir, bin_file, superbinary_file, metadata_file, release_notes_file, mfigr2_file
    bin_file = None
    superbinary_file = None
    metadata_file = None
    release_notes_file = None
    mfigr2_file = None
    for f in os.listdir(script_dir):
        if f.startswith('.'):
            continue
        if re.search(r'\.bin$', f, re.IGNORECASE):
            bin_file = f
        elif re.search(r'meta.*data.*\.plist$', f, re.IGNORECASE):
            metadata_file = f
        elif re.search(r'\.plist$', f, re.IGNORECASE):
            superbinary_file = f
        elif re.search(r'notes', f, re.IGNORECASE):
            release_notes_file = f
        elif re.search(r'^mfigr2$', f, re.IGNORECASE):
            mfigr2_file = f
        elif re.search(r'README\.rst|\.py[a-z]?|\.tmpl$', f, re.IGNORECASE):
            continue
        else:
            raise Exception(f'Don\'t know what to do with: {f}')
    if bin_file is None:
        raise Exception(f'Binary MCUBoot image is required.')
    if mfigr2_file is None:
        raise Exception(f'"mfigr2" tool is required.')
    print(f'''    Binary MCUBoot image:   {bin_file}
    SuperBinary plist file: {superbinary_file or "[default will be used]"}
    Meta Data plist file:   {metadata_file or "[default will be used]"}
    Release Notes:          {release_notes_file or "[none]"}
    mfigr2 tool:            {mfigr2_file}''')

def send_to_repo():
    global script_dir, bin_file, superbinary_file, metadata_file, release_notes_file, mfigr2_file
    root = None
    check_dir = script_dir
    for i in range(0, 10):
        if os.path.isdir(os.path.join(check_dir, '.git')):
            root = check_dir
            break
        check_dir = os.path.abspath(os.path.join(check_dir, '..'))
    if root is None:
        raise Exception('Cannot find git repository that this script belongs to.')
    git_params = ['git', 'add', '--']
    print(f'    Source repo:            {root}')
    temp_path = tempfile.mkdtemp()
    dest_path = f'{temp_path}/tools/samples/SuperBinary/github_action'
    shutil.copytree(f'{root}/.git', f'{temp_path}/.git')
    exec(['git', 'reset'], temp_path)
    os.makedirs(dest_path, exist_ok=True)
    shutil.copytree(f'{root}/.github', f'{temp_path}/.github')
    git_params.append('.github')
    shutil.copy(f'{script_dir}/trigger_action.py', dest_path)
    git_params.append(f'tools/samples/SuperBinary/github_action/trigger_action.py')
    shutil.copy(f'{script_dir}/SuperBinary.plist.tmpl', dest_path)
    git_params.append(f'tools/samples/SuperBinary/github_action/SuperBinary.plist.tmpl')
    shutil.copy(f'{script_dir}/{bin_file}', dest_path)
    git_params.append(f'tools/samples/SuperBinary/github_action/{bin_file}')
    shutil.copy(f'{script_dir}/{mfigr2_file}', dest_path)
    git_params.append(f'tools/samples/SuperBinary/github_action/{mfigr2_file}')
    if superbinary_file:
        shutil.copy(f'{script_dir}/{superbinary_file}', dest_path)
        git_params.append(f'tools/samples/SuperBinary/github_action/{superbinary_file}')
    if metadata_file:
        shutil.copy(f'{script_dir}/{metadata_file}', dest_path)
        git_params.append(f'tools/samples/SuperBinary/github_action/{metadata_file}')
    if release_notes_file and os.path.isdir(f'{script_dir}/{release_notes_file}'):
        shutil.copytree(f'{script_dir}/{release_notes_file}', f'{dest_path}/{release_notes_file}')
        git_params.append(f'tools/samples/SuperBinary/github_action/{release_notes_file}')
    if release_notes_file and os.path.isfile(f'{script_dir}/{release_notes_file}'):
        shutil.copy(f'{script_dir}/{release_notes_file}', dest_path)
        git_params.append(f'tools/samples/SuperBinary/github_action/{release_notes_file}')
    exec(git_params, cwd=temp_path)
    exec(['git', 'commit', '-m', commit_message], temp_path)
    remotes_str = exec(['git', 'remote', '-v'], temp_path, True)
    remotes = {}
    for remote in remotes_str.splitlines():
        m = re.search(r'^\s*(\S*)\s*(\S*)', remote)
        for forb in forbidden_repos:
            if re.search(forb, m[2]):
                break
        else:
            remotes[m[1]] = m[2]
    if len(remotes) > 1:
        print('More than one repo available. Choose one:')
        i = 1
        tab = {}
        for (a, b) in remotes.items():
            print(f'    {i}) {a}        {b}')
            tab[i] = a
            i += 1
        num = int(input())
        remote_name = tab[num]
    else:
        remote_name = list(remotes.keys())[0]

    if remote_name not in remotes:
        raise Exception('Invalid git remote name')

    exec(['git', 'push', '--force', remote_name, f'HEAD:refs/heads/{branch_name}'], temp_path)

    def del_rw(action, name, exc):
        os.chmod(name, stat.S_IWRITE)
        try:
            os.remove(name)
        except:
            try:
                os.rmdir(name)
                ok = True
            except:
                ok = False
            if not ok:
                raise

    shutil.rmtree(temp_path, onerror=del_rw)

    url_parts = urllib.parse.urlparse(remotes[remote_name])
    action_url = f'{url_parts.scheme}://{url_parts.hostname}{url_parts.path}'
    if action_url.endswith('.git'):
        action_url = action_url[0:-4]
    action_url += '/actions/workflows/build_the_file.yml'

    print('\nGitHub Action triggered to compose your SuperBinary on:\n')
    print(f'    {action_url}\n')

def compose():
    global script_dir, bin_file, superbinary_file, metadata_file, release_notes_file, mfigr2_file
    if superbinary_file == None:
        f = open('SuperBinary.plist.tmpl', 'r')
        xml = f.read()
        f.close()
        xml = xml.replace('{bin_file}', bin_file)
        f = open('SuperBinary.plist', 'w')
        f.write(xml)
        f.close()
        superbinary_file = 'SuperBinary.plist'
    os.chmod(mfigr2_file, 0o755)
    args = ['python3', '../../../ncsfmntools', 'SuperBinary', '--debug', '--mfigr2', f'./{mfigr2_file}', '--out-uarp', 'SuperBinary.uarp']
    args.append('--metadata')
    if metadata_file:
        args.append(metadata_file)
    else:
        args.append('MetaData.plist')
    if release_notes_file:
        args.append('--release-notes')
        args.append(release_notes_file)
    args.append(superbinary_file)
    out = exec(args, capture=True)
    os.makedirs('output', exist_ok=True)
    f = open('output/hashes.txt', 'w')
    f.write(out)
    f.close()
    shutil.copy('SuperBinary.uarp', 'output')
    shutil.copy(superbinary_file, 'output')
    if metadata_file:
        shutil.copy(metadata_file, 'output')
    else:
        shutil.copy('MetaData.plist', 'output')
    if release_notes_file:
        if os.path.isdir(release_notes_file):
            shutil.copy('ReleaseNotes.zip', 'output')
        else:
            shutil.copy(release_notes_file, 'output')
    exec(['git', 'push', 'origin', '--delete', branch_name])

def main():
    global script_dir
    script_dir = os.path.dirname(os.path.abspath(__file__))
    get_content()
    if 'exec_action' in sys.argv:
        compose()
    else:
        send_to_repo()

if __name__ == '__main__':
    main()
