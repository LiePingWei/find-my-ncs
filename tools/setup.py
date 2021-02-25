from setuptools import setup, find_packages

setup(name='ncsfmntools',
      python_requires=">=3.6",
      version='0.1',
      packages=find_packages(),
      include_package_data=True,
      install_requires=[
            'intelhex~=2.3.0',
            'six~=1.15.0',
            'pynrfjprog~=10.10.0'
      ],
      entry_points='''
              [console_scripts]
              ncsfmntools=ncsfmntools.scripts.cli:cli
      ''',
      package_data={
        "": ["*.*"],
      }
      )
