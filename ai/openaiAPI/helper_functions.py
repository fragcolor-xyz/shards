import json

def convert_json_to_jsonl(source_file_path, target_file_path):

    # Read the JSON file
    with open(source_file_path, 'r', encoding='utf-8') as source_file:
        data = json.load(source_file)  # This loads the JSON content as a Python list

    # Write to JSONL file
    with open(target_file_path, 'w', encoding='utf-8') as target_file:
        for item in data:
            # Convert each item to JSON string and write as a new line
            target_file.write(json.dumps(item) + '\n')
    return

