using System.Runtime.CompilerServices;

namespace GodotMonoDecomp;

public enum LogLevel
{
	// resets to the default log level, which is Debug in Debug builds and Info in Release builds
	Default = -1,
	Verbose = 0,
	Debug = 1,
	Info = 2,
	Warning = 3,
	Error = 4,
}

/// <summary>
/// A single log message, along with the call site that produced it.
/// <see cref="File"/> is already trimmed to a repo-relative path where possible.
/// The call site (<see cref="Function"/>, <see cref="File"/>, <see cref="Line"/>) is only populated for
/// <see cref="LogLevel.Warning"/> and above; for lower levels it is empty/0.
/// </summary>
public readonly record struct LogRecord(LogLevel Level, string Message, string Function, string File, int Line);

/// <summary>
/// Library-wide logging. By default, messages go to the console (Verbose/Debug/Info to stdout, Warning/Error to stderr).
/// A host (e.g. the NativeAOT FFI layer) can redirect all messages by installing a sink with <see cref="SetSink"/>.
/// The sink may be invoked from any thread.
///
/// The call site (member name, file and line) is captured automatically by the compiler via the caller-info
/// attributes; callers only need to pass the message.
/// </summary>
public static class Logging
{
	private static volatile Action<LogRecord>? sink = null;

	private static volatile bool IncludeCallSiteForNonWarningMessages = false;

#if DEBUG
	private const LogLevel defaultLogLevel = LogLevel.Debug;
#else
	private const LogLevel defaultLogLevel = LogLevel.Info;
#endif
	private static volatile LogLevel LogLevel = defaultLogLevel;

	/// <summary>
	/// Installs a sink that receives all log messages. Pass null to restore the default console sink.
	/// </summary>
	public static void SetSink(Action<LogRecord>? newSink, LogLevel logLevel, bool includeCallSiteForNonWarningMessages = false)
	{
		sink = newSink;
		IncludeCallSiteForNonWarningMessages = includeCallSiteForNonWarningMessages;
		LogLevel = logLevel == LogLevel.Default ? defaultLogLevel : logLevel;
	}

	public static bool HasCustomSink => sink != null;

	// Every public helper must forward its own captured call site to Log(); otherwise Log() would capture its own location.
	public static void Log(
		LogLevel level,
		string message,
		[CallerMemberName] string member = "",
		[CallerFilePath] string file = "",
		[CallerLineNumber] int line = 0)
	{
		if (level < LogLevel) {
			return;
		}
		// The call site is only used for warnings and errors (as in Godot, where print_line has no location),
		// so skip the path trimming for everything else.
		var record = level >= LogLevel.Warning || IncludeCallSiteForNonWarningMessages
			? new LogRecord(level, message, member, TrimPath(file), line)
			: new LogRecord(level, message, string.Empty, string.Empty, 0);
		var currentSink = sink;
		if (currentSink != null)
		{
			try
			{
				currentSink(record);
				return;
			}
			catch (Exception e)
			{
				// A misbehaving sink must never break decompilation; fall back to the console.
				try
				{
					Console.Error.WriteLine($"Log sink threw an exception: {e.Message}");
				}
				catch
				{
					// ignored
				}
			}
		}
		DefaultSink(record);
	}

	public static void Verbose(
		string message,
		[CallerMemberName] string member = "",
		[CallerFilePath] string file = "",
		[CallerLineNumber] int line = 0) => Log(LogLevel.Verbose, message, member, file, line);

	public static void Debug(
		string message,
		[CallerMemberName] string member = "",
		[CallerFilePath] string file = "",
		[CallerLineNumber] int line = 0) => Log(LogLevel.Debug, message, member, file, line);

	public static void Info(
		string message,
		[CallerMemberName] string member = "",
		[CallerFilePath] string file = "",
		[CallerLineNumber] int line = 0) => Log(LogLevel.Info, message, member, file, line);

	public static void Warn(
		string message,
		[CallerMemberName] string member = "",
		[CallerFilePath] string file = "",
		[CallerLineNumber] int line = 0) => Log(LogLevel.Warning, message, member, file, line);

	public static void Error(
		string message,
		[CallerMemberName] string member = "",
		[CallerFilePath] string file = "",
		[CallerLineNumber] int line = 0) => Log(LogLevel.Error, message, member, file, line);

	public static void Error(
		string message,
		Exception exception,
		[CallerMemberName] string member = "",
		[CallerFilePath] string file = "",
		[CallerLineNumber] int line = 0)
	{
		Log(LogLevel.Error, $"{message}: {exception.Message}{Environment.NewLine}{exception.StackTrace}", member, file, line);
	}

	private const string ModuleDirName = "modules/";
	private const string RepoDirName = "godot-mono-decomp/";

	// CallerFilePath is the absolute path on the build machine; trim it to a repo-relative path
	// (e.g. "godot-mono-decomp/GodotMonoDecomp/NuGetUtils.cs") so that output resembles Godot's __FILE__.
	private static string TrimPath(string file)
	{
		if (string.IsNullOrEmpty(file))
		{
			return file;
		}
		var normalized = file.Replace('\\', '/');
		var idx = normalized.LastIndexOf(ModuleDirName, StringComparison.Ordinal);
		if (idx < 0)
		{
			idx = normalized.LastIndexOf(RepoDirName, StringComparison.Ordinal);
		}
		if (idx >= 0)
		{
			return normalized.Substring(idx);
		}
		return normalized;
	}

	private static void DefaultSink(LogRecord record)
	{
		try
		{
			switch (record.Level)
			{
				case LogLevel.Warning:
					Console.Error.WriteLine($"WARNING: {record.Message}{FormatLocation(record)}");
					break;
				case LogLevel.Error:
					Console.Error.WriteLine($"ERROR: {record.Message}{FormatLocation(record)}");
					break;
				default:
					Console.WriteLine(record.Message);
					break;
			}
		}
		catch
		{
			// Console may be unavailable (e.g. closed handles when embedded); nothing else to do.
		}
	}

	// Mirrors Godot's layout:
	// WARNING: message
	//      at: Member (path/File.cs:123)
	private static string FormatLocation(LogRecord record)
	{
		if (string.IsNullOrEmpty(record.Function) && string.IsNullOrEmpty(record.File))
		{
			return string.Empty;
		}
		return $"{Environment.NewLine}     at: {record.Function} ({record.File}:{record.Line})";
	}
}
