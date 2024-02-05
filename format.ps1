$cppFiles = Get-ChildItem -Path ${PWD} -Recurse -Include *.cpp, *.hpp | 
    Where-Object { -not $_.FullName.Contains(".pio") } |
    ForEach-Object { 
        $relativePath = $_.FullName.Replace($PWD, '').Replace('\', '/')
        "/opt$relativePath"
    }

# Create a string that represents a shell command to format each file
$formatCommands = $cppFiles -join " " | ForEach-Object { "clang-format -i --style=microsoft $_" }

# Run the Docker container and pass the shell commands to be executed
docker run --rm --privileged=$true --volume ${PWD}:/opt --entrypoint /bin/sh saschpe/clang-format -c $formatCommands
