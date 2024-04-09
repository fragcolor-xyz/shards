import openai
import pandas as pd
import config
import os

# Setting the API key from the config file
os.environ['OPENAI_API_KEY'] = config.API_KEY

# Initialize the OpenAI client
openai.api_key = os.getenv("OPENAI_API_KEY")

file=open("../data/system_prompt_enhanced.txt","r")

system_prompt = file.read()

def run_models_on_prompts(prompts, models):
    """
    Runs given models on the provided prompts and returns the results in a DataFrame.

    :param prompts: List of text prompts.
    :param models: List of model names to use.
    :return: Pandas DataFrame with the results.
    """
    # Create empty DataFrame
    df = pd.DataFrame(columns=['Prompt', 'Model', 'Completion'])

    for model in models:
        for prompt in prompts:
            # Generate completion for each prompt using the specified model
            completion = openai.chat.completions.create(
                model=model,
                messages=[
                    {"role": "system", "content": f"{system_prompt}"}, # "" if using finetuned model # or content = f"{system_prompt}" if using base gpt model
                    {"role": "user", "content": f"{prompt}"}
                ],
                max_tokens=400
            )

            # Appending the result to the DataFrame
            df = pd.concat([df, pd.DataFrame({'Prompt': [prompt], 'Model': [model], 'Completion': [completion.choices[0].message.content]})], ignore_index=True)

    return df

prompts = ["spawn 6 trees in a grid", 
           "Generate 6 trees spread randomly on a 3x3 grid using d-TreePine, starting from x and z position of 1.0 Keep the y-axis at 0.0",
           "Generate 10 flowers spread randomly on a 2x2 grid using d-FlowerRed, starting from x and z position of 1.0 Keep the y-axis at 0.0 ",
           "Generate a 6x6 hexagonal grid using d-BlockHexagon tiles. Keep the y-axis at -1.0 Offset tile’s x position by 0.5 and z position by 0.865 so they form a cohesive hexagonal grid",
           "Starting from x position 4 and z position 4, generate a 4x4 hexagonal grid using d-BlockSnowHexagon tiles. Keep the y-axis at -1.0 Offset tile’s x position by 0.5 and z position by 0.865 so they form a cohesive hexagonal grid."
           ] 

# Models to run the prompts on
fine_tuned_model_name3_5 = "ft:gpt-3.5-turbo-0125:hasten-inc::9BymRBoU"
models = [
    fine_tuned_model_name3_5,
    "gpt-3.5-turbo", #==gpt-3.5-turbo-0125 #up to sep 2021 data # or fine-tuned model
    "gpt-4",  #== gpt-4-0125-preview. #up to dec 2023 data; gpt-4-1106-preview up to apr 2023 data 
]

# Running the models on the prompts
results_df = run_models_on_prompts(prompts, models)

# Saving the results to a CSV file
results_df.to_csv("gpt_model_completions.csv", index=False)

print("Completions saved to model_completions.csv")
