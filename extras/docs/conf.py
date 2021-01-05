import sys
import os
import shlex
import subprocess

read_the_docs_build = os.environ.get('READTHEDOCS', None) == 'True'

if read_the_docs_build:
    subprocess.call('doxygen', shell=True)

extensions = ['breathe']
breathe_projects = { 'confrm-arduino-esp32': 'xml' }
breathe_default_project = "confrm-arduino-esp32"
templates_path = ['_templates']
source_suffix = '.rst'
master_doc = 'index'
project = u'confrm-arduino-esp32'
copyright = u'2021, confrm.io'
author = u'Martynp'
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
  (master_doc, 'Confrm.tex', u'confrm-arduino-esp32 Documentation',
      u'confrm-arduino-esp32', 'manual'),
]
