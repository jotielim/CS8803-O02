import time
import os
import sys
import argparse
import json
from bonnie.submission import Submission


def main():
  parser = argparse.ArgumentParser(description='Submits code to the Udacity site.')
  parser.add_argument('--provider', choices = ['gt', 'udacity'], default = 'gt')
  parser.add_argument('--environment', choices = ['local', 'development', 'staging', 'production'], default = 'production')

  args = parser.parse_args()

  files = ['pr2_sandbox.c']

  quiz = 'pr2_sandbox'

  app_data_dir = os.path.abspath(".bonnie")

  submission = Submission('cs8803-02', quiz, 
                          filenames = files, 
                          environment = args.environment, 
                          provider = args.provider)

  while not submission.poll():
    time.sleep(3.0)

  if submission.result():
    print submission.result()
  elif submission.error_report():
    print submission.error_report()
  else:
    print "Unknown error."

if __name__ == '__main__':
  main()
