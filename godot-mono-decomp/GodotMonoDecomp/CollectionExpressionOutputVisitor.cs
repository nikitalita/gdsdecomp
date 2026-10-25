using System.Collections.Generic;
using System.IO;
using System.Linq;
using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.DebugInfo;
using ICSharpCode.Decompiler.CSharp.OutputVisitor;
using ICSharpCode.Decompiler.CSharp.Syntax;
using ICSharpCode.Decompiler.IL;
using ICSharpCode.Decompiler.CSharp;
using ICSharpCode.Decompiler.TypeSystem;
using ICSharpCode.Decompiler.Disassembler;
using ICSharpCode.Decompiler.Metadata;
using ICSharpCode.ILSpyX.Extensions;
using SequencePoint = ICSharpCode.Decompiler.DebugInfo.SequencePoint;
using System.Reflection.Metadata;

namespace GodotMonoDecomp;

internal sealed class CollectionExpressionArrayAnnotation
{
	public static readonly CollectionExpressionArrayAnnotation Instance = new();

	private CollectionExpressionArrayAnnotation()
	{
	}
}

internal sealed class CollectionExpressionSpreadElementAnnotation
{
	public static readonly CollectionExpressionSpreadElementAnnotation Instance = new();

	private CollectionExpressionSpreadElementAnnotation()
	{
	}
}
public class ProxyOutput: ITextOutput
{
		public ITextOutput realOutput;
		public ProxyOutput(ITextOutput realOutput)
		{
			this.realOutput = realOutput;
		}
		public string IndentationString {
			get { return realOutput.IndentationString; }
			set { realOutput.IndentationString = value; }
		}
		public void Indent()
		{
			realOutput.Indent();
		}
		public void Unindent()
		{
			realOutput.Unindent();
		}
		public void Write(char ch)
		{
			realOutput.Write(ch);
		}
		public void Write(string text)
		{
			realOutput.Write(text);
		}
		public void WriteLine()
		{
			realOutput.WriteLine();
		}
		public void WriteReference(OpCodeInfo opCode, bool omitSuffix = false)
		{
			realOutput.WriteReference(opCode, omitSuffix);
		}
		public void WriteReference(MetadataFile metadata, Handle handle, string text, string protocol = "decompile", bool isDefinition = false)
		{
			realOutput.WriteReference(metadata, handle, text, protocol, isDefinition);
		}
		public void WriteReference(IType type, string text, bool isDefinition = false)
		{
			realOutput.WriteReference(type, text, isDefinition);
		}
		public void WriteReference(IMember member, string text, bool isDefinition = false)
		{
			realOutput.WriteReference(member, text, isDefinition);
		}
		public void WriteLocalReference(string text, object reference, bool isDefinition = false, bool isHoverOnly = false)
		{
			realOutput.WriteLocalReference(text, reference, isDefinition, isHoverOnly);
		}

		public void MarkDefinitionStart()
		{
			realOutput.MarkDefinitionStart();
		}

		public void MarkFoldStart(string collapsedText = "...", bool defaultCollapsed = false, bool isDefinition = false)
		{
			realOutput.MarkFoldStart(collapsedText, defaultCollapsed, isDefinition);
		}
		public void MarkFoldEnd()
		{
			realOutput.MarkFoldEnd();
		}
}

public class GodotLineDisassembler: MethodBodyDisassembler
{

	ITextOutput fakeOutput;
	ITextOutput realOutput;

	ProxyOutput proxyOutput;
	IList<SequencePoint> sequencePoints;
	public GodotLineDisassembler(ITextOutput output, CancellationToken cancellationToken, List<SequencePoint> sequencePointsToDecompile) : this(new ProxyOutput(new PlainTextOutput()), output, cancellationToken, sequencePointsToDecompile)
	{
	}

	private GodotLineDisassembler(ProxyOutput proxyOutput, ITextOutput output, CancellationToken cancellationToken, List<SequencePoint> sequencePointsToDecompile) : base(proxyOutput, cancellationToken){
		this.fakeOutput = proxyOutput.realOutput;
		this.proxyOutput = proxyOutput;
		this.sequencePoints = sequencePointsToDecompile;
		this.realOutput = output;
	}

	public override void Disassemble(MetadataFile module, System.Reflection.Metadata.MethodDefinitionHandle handle)
	{
		base.Disassemble(module, handle);
	}

	protected override void WriteInstruction(ITextOutput _, MetadataFile metadataFile, System.Reflection.Metadata.MethodDefinitionHandle methodHandle, ref System.Reflection.Metadata.BlobReader blob, int methodRva)
	{
		int offset = blob.Offset;
		if (sequencePoints.Any(seq => (seq.Offset <= offset && seq.EndOffset > offset)))
		{
			this.proxyOutput.realOutput = realOutput;
			base.WriteInstruction(realOutput, metadataFile, methodHandle, ref blob, methodRva);
		}
		else
		{
			this.proxyOutput.realOutput = fakeOutput;
			base.WriteInstruction(fakeOutput, metadataFile, methodHandle, ref blob, methodRva);
		}
		this.proxyOutput.realOutput = fakeOutput;
	}
}

public class GodotCSharpOutputVisitor : CSharpOutputVisitor
{
	private readonly bool emitILAnnotationComments;
	private readonly TokenWriter commentWriter;

	private readonly GodotMonoDecompSettings settings;

	private Dictionary<ILFunction, List<ICSharpCode.Decompiler.DebugInfo.SequencePoint>> sequencePoints = [];

	private Dictionary<int, List<KeyValuePair<ILFunction, List<ICSharpCode.Decompiler.DebugInfo.SequencePoint>>>> startLineToSequencePoints = [];

	private readonly CSharpDecompiler? decompiler;

	private int lastStartLine = 0;
	private readonly Dictionary<(MetadataFile Module, MethodDefinitionHandle MethodHandle, string SequenceKey), string[]> disassemblyLineCache = [];

	public GodotCSharpOutputVisitor(TextWriter w, GodotMonoDecompSettings settings, bool emitILAnnotationComments = false, CSharpDecompiler? decompiler = null)
		: this(new TextWriterTokenWriter(w) { IndentationString = settings.CSharpFormattingOptions.IndentationString }, settings, emitILAnnotationComments, decompiler)
	{
	}

	private GodotCSharpOutputVisitor(TokenWriter w, GodotMonoDecompSettings settings,
		bool emitILAnnotationComments, CSharpDecompiler? decompiler) : base(w, settings.CSharpFormattingOptions)
	{
		this.decompiler = decompiler;
		this.emitILAnnotationComments = emitILAnnotationComments;
		this.settings = settings;
		this.commentWriter = w;
	}

	// we have to create another visitor and output to a fake writer in order
	// for the output visitor to annotate all the nodes with line/column information
	// So that we can use this to generate sequence points for the IL instructions
	static void WriteCode(TextWriter output, GodotMonoDecompSettings settings, SyntaxTree syntaxTree, IDecompilerTypeSystem typeSystem)
	{
		syntaxTree.AcceptVisitor(new InsertParenthesesVisitor { InsertParenthesesForReadability = true });
		TokenWriter tokenWriter = new TextWriterTokenWriter(output) { IndentationString = settings.CSharpFormattingOptions.IndentationString };
		tokenWriter = TokenWriter.WrapInWriterThatSetsLocationsInAST(tokenWriter);
		syntaxTree.AcceptVisitor(new GodotCSharpOutputVisitor(tokenWriter, settings, false, null));
	}

	public override void VisitSyntaxTree(SyntaxTree syntaxTree)
	{
		if (emitILAnnotationComments && decompiler != null) {
			lastStartLine = 0;
			startLineToSequencePoints.Clear();
			sequencePoints.Clear();
			disassemblyLineCache.Clear();
			var fakeWriter = new StringWriter();
			WriteCode(fakeWriter, settings, syntaxTree, decompiler.TypeSystem);
			sequencePoints = decompiler.CreateSequencePoints(syntaxTree);

			// create a
			var startLines = sequencePoints.Where(kvp => kvp.Value.Count > 0).SelectMany(kvp => kvp.Value.Select(seq => seq.StartLine)).Distinct().OrderBy(l => l).ToList();
			foreach (var startLine in startLines)
			{
				var sequencePointsForLine = sequencePoints.Select(kvp => {
					return new KeyValuePair<ILFunction, List<ICSharpCode.Decompiler.DebugInfo.SequencePoint>>(kvp.Key, kvp.Value.Where(seq => seq.StartLine == startLine).ToList());
				}).Where(kvp => kvp.Value.Count > 0).ToList();
				startLineToSequencePoints[startLine] = sequencePointsForLine;
			}
		}
		base.VisitSyntaxTree(syntaxTree);
	}

	protected override void StartNode(AstNode node)
	{
		var startLine = node.StartLocation.Line;
		if (emitILAnnotationComments && startLine > lastStartLine && startLineToSequencePoints.ContainsKey(startLine)) {
			var sequencePointsForLine = startLineToSequencePoints[startLine];
			// transform it into a list of (methodHandle, sequencePoint)
			var sps = sequencePointsForLine
				.SelectMany(kvp => kvp.Value.Select(sp => (Method: kvp.Key, SequencePoint: sp)))
				.OrderBy(sp => sp.SequencePoint.Offset)
				.ToList();
			var methodHandles = new Dictionary<ILFunction, (MetadataFile Module, MethodDefinitionHandle MethodHandle)>();
			foreach (var sp in sps)
			{
				if (methodHandles.ContainsKey(sp.Method))
				{
					continue;
				}

				if (TryGetMethodDefinitionHandle(sp.Method, out var metadataFile, out var methodHandle))
				{
					methodHandles[sp.Method] = (metadataFile, methodHandle);
				}
			}

			var emittedSequencePoints = new HashSet<(MetadataFile Module, MethodDefinitionHandle MethodHandle, int Offset, int EndOffset)>();
			foreach (var sp in sps)
			{
				if (!methodHandles.TryGetValue(sp.Method, out var resolvedMethod))
				{
					continue;
				}

				var emitKey = (resolvedMethod.Module, resolvedMethod.MethodHandle, sp.SequencePoint.Offset, sp.SequencePoint.EndOffset);
				if (!emittedSequencePoints.Add(emitKey))
				{
					continue;
				}

				var lines = GetDisassemblyLines(resolvedMethod.Module, resolvedMethod.MethodHandle, [sp.SequencePoint]);
				if (lines.Length == 0)
				{
					continue;
				}

				commentWriter.NewLine();
				foreach (var line in lines)
				{
					commentWriter.WriteComment(CommentType.SingleLine, line);
				}
			}
		}
		lastStartLine = startLine;
		base.StartNode(node);
	}

	private string[] GetDisassemblyLines(MetadataFile metadataFile, MethodDefinitionHandle methodHandle, List<SequencePoint> sequencePointList)
	{
		var sequenceKey = string.Join(";", sequencePointList.Select(sp => $"{sp.Offset:x4}-{sp.EndOffset:x4}"));
		var cacheKey = (metadataFile, methodHandle, sequenceKey);
		if (disassemblyLineCache.TryGetValue(cacheKey, out var cachedLines))
		{
			return cachedLines;
		}

		var output = new PlainTextOutput();
		var methodDisassembler = new GodotLineDisassembler(output, default(CancellationToken), sequencePointList);
		methodDisassembler.Disassemble(metadataFile, methodHandle);
		var lines = output
			.ToString()
			.Split('\n')
			.Where(line => line.Length > 0)
			.ToArray();
		disassemblyLineCache[cacheKey] = lines;
		return lines;
	}

	private static bool TryGetMethodDefinitionHandle(ILFunction function, out MetadataFile metadataFile, out MethodDefinitionHandle methodHandle)
	{
		metadataFile = null!;
		methodHandle = default;

		var method = function.Method;
		var token = method?.MetadataToken ?? default;
		var module = method?.ParentModule?.MetadataFile;
		if (module == null || token.IsNil || token.Kind != HandleKind.MethodDefinition)
		{
			return false;
		}

		metadataFile = module;
		methodHandle = (MethodDefinitionHandle)token;
		return true;
	}

	public override void VisitArrayInitializerExpression(ArrayInitializerExpression arrayInitializerExpression)
	{
		if (arrayInitializerExpression.Annotation<CollectionExpressionArrayAnnotation>() == null)
		{
			base.VisitArrayInitializerExpression(arrayInitializerExpression);
			return;
		}

		StartNode(arrayInitializerExpression);
		WriteToken(Tokens.LBracket);

		bool first = true;
		foreach (var element in arrayInitializerExpression.Elements)
		{
			if (!first)
			{
				WriteToken(Tokens.Comma);
				Space();
			}

			if (element.Annotation<CollectionExpressionSpreadElementAnnotation>() != null)
			{
				WriteToken(BinaryOperatorExpression.RangeToken);
			}

			element.AcceptVisitor(this);
			first = false;
		}

		WriteToken(Tokens.RBracket);
		EndNode(arrayInitializerExpression);
	}
}
