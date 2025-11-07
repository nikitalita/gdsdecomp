// Copyright (c) 2011 AlphaSierraPapa for the SharpDevelop Team
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify, merge,
// publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
// to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

using ICSharpCode.Decompiler.CSharp;
using ICSharpCode.Decompiler.CSharp.Syntax;
using ICSharpCode.Decompiler.CSharp.Transforms;
using ICSharpCode.Decompiler.TypeSystem;

namespace GodotMonoDecomp;

/// <summary>
/// A hack to remove compiler-generated exception throws caused by switch expressions being compiled as imperative code.
/// e.g.:
/// ```c#
/// int x = 5;
/// float num = x switch
/// {
/// 	5 => 0.8f
/// };
/// ```
///
/// gets compiled to:
///
/// ```c#
/// float num = default(float);
/// switch (difficultyLevel)
/// {
/// 	case 5:
/// 		num = 0.8f;
/// 		break;
/// 	default:
/// 		throw new global::_003CPrivateImplementationDetails_003E.ThrowSwitchExpressionException(difficultyLevel);
/// 		break;
/// }
///```
/// </summary>
public class RemoveGeneratedExceptionThrows : DepthFirstAstVisitor, IAstTransform
{
	public void Run(AstNode rootNode, TransformContext context)
	{
		rootNode.AcceptVisitor(this);
	}
	//we're looking for calls to global::_003CPrivateImplementationDetails_003E.ThrowSwitchExpressionException(difficultyLevel);
	// We want to replace it with "throw new SwitchExpressionException(difficultyLevel);"
	public override void VisitInvocationExpression(InvocationExpression methodInvocation)
	{
		if (methodInvocation.Target is MemberReferenceExpression member)
		{
			var firstChild = member.FirstChild?.ToString() ?? "";
			if (firstChild.StartsWith("global::") && firstChild.Contains("PrivateImplementationDetails"))
			{
				var lastChild = member.LastChild?.ToString() ?? "";
				if (lastChild.StartsWith("Throw"))
				{
					var exceptionName = "System.Runtime.CompilerServices." + lastChild.Substring(5);
					var arguments = methodInvocation.LastChild;
					if (arguments is Expression expression)
					{
						AstType type = new SimpleType(exceptionName);
						var throwExpression = new ThrowExpression(new ObjectCreateExpression(type, expression.Clone()));
						methodInvocation.ReplaceWith(throwExpression);
						base.VisitThrowExpression(throwExpression);
						return;
					}
				}
			}
		}
		base.VisitInvocationExpression(methodInvocation);
	}
}
