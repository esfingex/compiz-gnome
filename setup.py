from setuptools import setup, find_packages

setup(
    name="compiz_gnome_tools",
    version="0.1.0",
    description="Python Management Tools and Orchestration Utilities for compiz-gnome",
    author="esfingex & compiz-gnome Contributors",
    license="GPL-3.0",
    packages=find_packages(),
    python_requires=">=3.10",
    install_requires=[
        "flatbuffers>=23.5.26",
        "protobuf>=4.25.0",
    ],
    entry_points={
        "console_scripts": [
            "compiz-cli=tools.cli:main",
        ],
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Environment :: X11 Applications :: GNOME",
        "License :: OSI Approved :: GNU General Public License v3 (GPLv3)",
        "Programming Language :: Python :: 3.10",
        "Topic :: Desktop Environment :: Window Managers",
    ],
)
