# elastic-bench
The cloud analytic benchmark than generates workloads for elasticity evaluations.

The key function of the benchmark is to allow developers to evaluate and optimize the database. However, CAB is not convenient for database developers to conduct tests.

Our code is somewhat based on CAB, but we have made the following modifications and optimizations to enhance functionality:

1.Removed the randomness in mode selection and fixed the generation of query flows for 5 different modes.

2.The original code only used CPU time to fit the modes and generate query flows. We added additional dimensions for users to choose from in generating these dimensions.

3.The original code had high randomness in the mode generation process, and it was only based on data analysis from a single day of the dataset. We now directly randomly select data from the dataset to fit the model.

4.The original code was not friendly for load prediction and required the DBMS to run for a long time to simulate the query flow again. After optimization, we now directly provide the database information from previous queries instead of regenerating the query flow.

The content with bug fixes will be committed today.
