#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Module allowing to load JON-schema file and validate JSON files.
"""

import argparse
import json
import logging
import os
import pathlib
import sys
import typing

try:
    import jsonschema
except ImportError:
    logging.critical("Could not import jsonschema, please try pip(3) install jsonschema")
    sys.exit(1)
    

def create_validator(schema_file_name: typing.Union[str, os.PathLike, pathlib.Path],
                     stream_or_level: typing.Union[str, logging.Logger] = "INFO")->jsonschema.protocols.Validator:
    """Function trying to create a validator object from a JSON-schema file.

    This function tries to create a validator instance from the provided JSON-schema file.

    :param schema_file_name: path of the JSON-schema file to be loaded.
    :param stream_or_level: logging object or log level for the function.

    :raise: TypeError if the type of an argument is not correct.
    :raise: FileNotFoundError if the specified file cannot be found.
    :raise: ValueError if the provided schema file is invalid.

    :return: the validator instance in which the JSON-schema was loaded.
    """

    if isinstance(stream_or_level, str):
        stream = logging.getLogger(name="create_validator", level = stream_or_level)
    elif isinstance(stream_or_level, logging.Logger):
        stream = stream_or_level
    else:
        raise TypeError("stream_or_level argument must either be a str or a logging.Logger instance")

    stream.debug("Loading schema from %s file", schema_file_name)

    if not os.path.isfile(schema_file_name):
        raise FileNotFoundError("File %s doesn't exist", schema_file_name)

    with open(schema_file_name, "r") as schema_file:
        schema_data = json.load(schema_file)

    try:
        jsonschema.validators.Draft202012Validator.check_schema(schema=schema_data)
    except jsonschema.exceptions.SchemaError as invalid_schema:
        stream.error(invalid_schema)
        raise ValueError("Provided schema is invalid, aborting")

    stream.debug("Schema successfully loaded, creating validator")

    validator = jsonschema.validators.Draft202012Validator(schema=schema_data)

    stream.debug("Schema validator successfully instanciated")

    return validator


def validate_files(validator: jsonschema.protocols.Validator,
                   file_names: typing.Union[typing.List[typing.Union[str, os.PathLike, pathlib.Path]], str, os.PathLike, pathlib.Path],
                   stream_or_level: typing.Union[str, logging.Logger] = "INFO")->typing.List[bool]:
    """Function analysing files with the provided validator.

    This function analyses the given file(s) with the provided validator instance.

    :param validator: validator instance, as created by create_validator.
    :param file_names: (list of) file name(s) to be analysed.
    :param stream_or_level: logging object or log level for the function.
    
    :raise: TypeError if the type of an argument is not correct.

    :return: a list of bool. Each element is the result of the corresponding
    element in the input `file_names` list.
    """

    if isinstance(stream_or_level, str):
        stream = logging.getLogger(name="create_validator", level = stream_or_level)
    elif isinstance(stream_or_level, logging.Logger):
        stream = stream_or_level
    else:
        raise TypeError("stream_or_level argument must either be a str or a logging.Logger instance")

    if not isinstance(validator, jsonschema.protocols.Validator):
        raise TypeError("validator argument must be a jsonschema.protocols.Validator instance")

    if isinstance(file_names, (str, os.PathLike, pathlib.Path)):
        file_names = [file_names]
    elif isinstance(file_names, list) and isinstance(file_names[0], (str, os.PathLike, pathlib.Path)):
        pass
    else:
        raise TypeError("file_names argument must be a list of str, os.PathLike or pathlib.Path instances")

    result = []

    for file_name in file_names:
        stream.debug("Analysing file %s", file_name)

        with open(file_name) as data_file:
            data = json.load(data_file)

        if len(data) == 0:
            stream.warning("File %s does not contain any data", file_name)
            result.append(False)
            continue

        result.append(validator.is_valid(data))

        if not result[-1]:
            if stream.getEffectiveLevel() > logging.DEBUG:
                stream.error("Error(s) found in %s", file_name)
            for error in validator.iter_errors(data):
                stream.error(error.message)
                
        stream.debug("File %s analysed", file_name)
        
        if stream.getEffectiveLevel() <= logging.DEBUG:
            print()

    return result


if __name__ == "__main__":
    
    arg_parser = argparse.ArgumentParser(description="Script checking JSON file syntax from a json-schema",
                                         epilog="Copyright (c) 2024 Atracsys LLC",
                                         formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    arg_parser.add_argument("-l", "--level", metavar="L", action="store", dest="output_level", type=str,
                            help="""set the output verbosity level""", default="INFO",
                            choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"])
    arg_parser.add_argument("-s", "--schema", metavar="F", action="store", dest="schema_file", type=str,
                            help="""set the input schema file to be used for validation""", required=True)
    arg_parser.add_argument(metavar="F", nargs="+", action="store", dest="input_files", type=str,
                            help="""set the input file(s) to be validated""")

    the_args = arg_parser.parse_args()

    logger = logging.Logger(name="validate_geometry_file")
    logger.setLevel(level=the_args.output_level)
    formatter = logging.Formatter(fmt="%(asctime)s {%(name)s} [%(levelname)-8s] %(message)s (%(filename)s:%(lineno)d)")
    handler = logging.StreamHandler()
    handler.setLevel(level=the_args.output_level)
    handler.setFormatter(formatter)
    logger.addHandler(handler)
    

    try:
        validator = create_validator(schema_file_name= the_args.schema_file,
                                     stream_or_level= logger)
    except TypeError as arg_err:
        logger.critical(arg_err)
        sys.exit(1)
    except FileNotFoundError as file_err:
        logger.critical(file_err)
        sys.exit(1)
    except ValueError as schema_err:
        logger.critical(schema_err)
        sys.exit(1)

    if logger.getEffectiveLevel() <= logging.DEBUG:
        print()

    try:
        results = validate_files(validator= validator,
                                 file_names= the_args.input_files,
                                 stream_or_level= logger)
    except TypeError as validator_err:
        logger.critical(validator_err)
        sys.exit(1)

    for name, status in zip(the_args.input_files, results):
        if status:
            logger.info("File %s is valid", name)
        else:
            logger.warning("File %s is invalid", name)

    sys.exit(0)
