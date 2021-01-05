import sys
import os
import shlex
import subprocess

read_the_docs_build = os.environ.get('READTHEDOCS', None) == 'True'

if read_the_docs_build:
    subprocess.call('doxygen', shell=True)

extensions = ['breathe']
breathe_projects = { 'Confrm': 'xml' }
breathe_default_project = "Confrm"
templates_path = ['_templates']
source_suffix = '.rst'
master_doc = 'index'
project = u'Confrm'
copyright = u'2021, Confrm'
author = u'Confrm'
version = '0.1'
release = '0.1'
language = None
exclude_patterns = ['_build']
pygments_style = 'sphinx'
todo_include_todos = False
html_static_path = ['_static']
htmlhelp_basename = 'Confrmdoc'
latex_elements = {
}
latex_documents = [
  (master_doc, 'Confrm.tex', u'Confrm Documentation',
   u'Confrm', 'manual'),
]
