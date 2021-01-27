from setuptools import setup, find_packages

setup(name='ncsfmntools',
      version='0.1',
      packages=find_packages(),
      include_package_data=True,
      install_requires=[
            'click~=7.1.2',
            'intelhex~=2.3.0',
            'Jinja2~=2.9.6',
            'six~=1.15.0'
      ],
      entry_points='''
              [console_scripts]
              ncsfmntools=ncsfmntools.cli:cli
      ''',
      )