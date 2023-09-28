#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-4-Clause
#

import re
from setuptools import setup, find_packages

def version_extract(package_info_content):
      result = re.search(r"^__version__ = ['\"]([^'\"]*)['\"]", package_info_content, re.M)
      if result:
            return result.group(1)
      else:
            raise RuntimeError("ncsfmntools: unable to find version string in %s." % PACKAGE_INFO_FILE)

PACKAGE_INFO_FILE = "ncsfmntools/__package_info__.py"
package_info_content = open(PACKAGE_INFO_FILE, "rt").read()
version = version_extract(package_info_content)

setup(name='ncsfmntools',
      python_requires=">=3.6",
      version=version,
      packages=find_packages(),
      include_package_data=True,
      install_requires=[
            'intelhex~=2.3.0',
            'six~=1.15',
            'pynrfjprog~=10.10'
      ],
      entry_points='''
              [console_scripts]
              ncsfmntools=ncsfmntools.scripts.cli:cli
      ''',
      package_data={
        "": ["*.*"],
      }
)
