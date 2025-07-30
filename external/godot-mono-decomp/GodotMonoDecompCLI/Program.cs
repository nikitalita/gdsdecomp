// See https://aka.ms/new-console-template for more information

using GodotMonoDecomp;

// specify this is the main entry point
int Main(string[] args)
{

    if (args.Length < 3)
    {
        Console.WriteLine("Usage: GodotMonoDecomp-cli <assembly-path> <output-path> <project-path> [<reference-paths>...]");
        return 1;
    }
    // get the assembly path from the command line
    string assemblyPath = args[0];

    // get the output path from the command line
    string outputPath = args[1];

    string projectPath = args[2];
    // get the reference paths from the command line
    string[] referencePaths = args.Skip(3).ToArray();

    // call the DecompileProject function
    int result = Lib.DecompileProject(assemblyPath, outputPath, projectPath, referencePaths);

    return result;
}

return Main(args);