#include <vtkAssemblyPath.h>
#include <vtkCell.h>
#include <vtkCommand.h>
#include <vtkCornerAnnotation.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkImageViewer2.h>
#include <vtkInteractorStyleImage.h>
#include <vtkJPEGReader.h>
#include <vtkPointData.h>
#include <vtkPropPicker.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkTextProperty.h>

// The mouse motion callback, to pick the image and recover pixel values
class vtkImageInteractionCallback : public vtkCommand
{
  public:

    static vtkImageInteractionCallback *New() 
    {
      return new vtkImageInteractionCallback; 
    }

    vtkImageInteractionCallback() 
    {
      this->Viewer = 0;
      this->Picker = 0;
      this->Annotation = 0;
      this->PointData = vtkPointData::New();
    }

    ~vtkImageInteractionCallback()
    {
      this->Viewer = 0;
      this->Picker = 0;
      this->Annotation = 0;
      this->PointData->Delete();
    }

    void SetPicker(vtkPropPicker *picker) 
    {
      this->Picker = picker; 
    }

    void SetAnnotation(vtkCornerAnnotation *annotation) 
    {
        this->Annotation = annotation; 
    }

    void SetViewer(vtkImageViewer2 *viewer) 
    {
      this->Viewer = viewer; 
    }


    virtual void Execute(vtkObject *, unsigned long event, void *)
    {
      this->Viewer;
      vtkRenderWindowInteractor *interactor = this->Viewer->GetRenderWindow()->GetInteractor();
      vtkRenderer* renderer = this->Viewer->GetRenderer();
      vtkImageActor* actor = this->Viewer->GetImageActor();
      vtkImageData* image = this->Viewer->GetInput();
      vtkInteractorStyle *style = vtkInteractorStyle::SafeDownCast(
          interactor->GetInteractorStyle());

      image->Update();

      // Pick at the mouse location provided by the interactor
      this->Picker->Pick( interactor->GetEventPosition()[0],
                          interactor->GetEventPosition()[1], 0.0, renderer );
      cout << "Mouse location: " << interactor->GetEventPosition()[0] << " , " << interactor->GetEventPosition()[1] << endl;
      // There could be other props assigned to this picker, so 
      // make sure we picked the image actor
      vtkAssemblyPath* path = this->Picker->GetPath();
      vtkProp* pickedProp = NULL;

      
      if( path )
        {
          cout << "There are " << path->GetNumberOfItems() << " items on the path." << endl;
        vtkCollectionSimpleIterator sit;
        path->InitTraversal( sit );
        vtkAssemblyNode *node;
        for( int i = 0; i < path->GetNumberOfItems(); ++i )
          {
          node = path->GetNextNode( sit );
          pickedProp = node->GetViewProp();
          if( actor == vtkImageActor::SafeDownCast( pickedProp ) )
            {
            break;
            }
          }
        }

      if( !pickedProp )
        {
        this->Annotation->SetText( 0, "Off Image" );
        interactor->Render();
        // Pass the event further on
        style->OnMouseMove();
        return;
        }

      // Get the world coordinates of the pick
      double pos[3];
      this->Picker->GetPickPosition( pos );
      // Fixes some numerical problems with the picking
      double *bounds = actor->GetDisplayBounds();
      int axis = this->Viewer->GetSliceOrientation();
      pos[axis] = bounds[2*axis];

      vtkPointData* pd = image->GetPointData();
      if( !pd )
        {
        return;
        }

      this->PointData->InterpolateAllocate( pd, 1, 1 );

      // Use tolerance as a function of size of source data
      double tol2 = image->GetLength();
      tol2 = tol2 ? tol2*tol2 / 1000.0 : 0.001;

      // Find the cell that contains pos
      int subId;
      double pcoords[3], weights[8];
      vtkCell* cell = image->FindAndGetCell(
          pos, NULL, -1, tol2, subId, pcoords, weights );
      if( cell )
        {
        // Interpolate the point data
        this->PointData->InterpolatePoint( pd, 0, cell->PointIds, weights );
        int components = this->PointData->GetScalars()->GetNumberOfComponents();
        double* tuple = this->PointData->GetScalars()->GetTuple( 0 );

        vtkStdString message = "Location: ( ";
        message += vtkVariant( pos[0] ).ToString();
        message += ", ";
        message += vtkVariant( pos[1] ).ToString();
        message += ", ";
        message += vtkVariant( pos[2] ).ToString();
        message += " )\nValue: ( ";

        for( int c = 0; c < components; ++c )
          {
          message += vtkVariant( tuple[ c ] ).ToString();
          if( c != components - 1 ) 
            {
            message += ", ";
            }
          }
        message += " )";
        this->Annotation->SetText( 0, message.c_str() );
        interactor->Render();
        style->OnMouseMove();
        }
    }

  private:

  // Pointer to the viewer
  vtkImageViewer2 *Viewer;

  // Pointer to the picker
  vtkPropPicker *Picker;

  // Pointer to the annotation
  vtkCornerAnnotation *Annotation;

  // Interpolator
  vtkPointData* PointData;
};


int main ( int argc, char* argv[] )
{
  //Verify input arguments
  if ( argc != 2 )
    {
    cout << "Required parameters: (jpeg) Filename" << endl << "Exiting..." << endl;
    return EXIT_FAILURE;
    }

  //Parse input argument
  vtkStdString inputFilename = argv[1];

  //Read the image
  vtkSmartPointer<vtkJPEGReader> jpegReader = 
      vtkSmartPointer<vtkJPEGReader>::New();
  if( !jpegReader->CanReadFile( inputFilename.c_str() ) )
    {
    cout << "Error reading file " << inputFilename << endl << "Exiting..." << endl;
    return EXIT_FAILURE;
    }
  jpegReader->SetFileName ( inputFilename.c_str() );

  // Picker to pick pixels
  vtkSmartPointer<vtkPropPicker> propPicker = 
      vtkSmartPointer<vtkPropPicker>::New();
  propPicker->PickFromListOn();

  // Give the picker a prop to pick
  vtkSmartPointer<vtkImageViewer2> imageViewer = 
      vtkSmartPointer<vtkImageViewer2>::New();
  propPicker->AddPickList( imageViewer->GetImageActor() );

  // Visualize
  vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor = 
      vtkSmartPointer<vtkRenderWindowInteractor>::New();
  imageViewer->SetInputConnection( jpegReader->GetOutputPort() );
  imageViewer->SetupInteractor( renderWindowInteractor );
  imageViewer->SetSize( 600, 600 );
  vtkRenderer* renderer = imageViewer->GetRenderer();
  renderer->ResetCamera();
  renderer->GradientBackgroundOn();
  renderer->SetBackground(0,0,0);
  renderer->SetBackground2(1,1,1);

  // Annotate the image with window/level and mouse over pixel information
  vtkSmartPointer<vtkCornerAnnotation> cornerAnnotation = 
      vtkSmartPointer<vtkCornerAnnotation>::New();
  cornerAnnotation->SetLinearFontScaleFactor( 2 );
  cornerAnnotation->SetNonlinearFontScaleFactor( 1 );
  cornerAnnotation->SetMaximumFontSize( 20 );
  cornerAnnotation->SetText( 0, "Off Image" );
  cornerAnnotation->SetText( 3, "<window>\n<level>" );
  cornerAnnotation->GetTextProperty()->SetColor( 1,0,0);

  imageViewer->GetRenderer()->AddViewProp( cornerAnnotation );

  // Callback listens to MouseMoveEvents invoked by the interactor's style
  vtkSmartPointer<vtkImageInteractionCallback> callback = 
      vtkSmartPointer<vtkImageInteractionCallback>::New();
  callback->SetViewer( imageViewer );
  callback->SetAnnotation( cornerAnnotation );
  callback->SetPicker( propPicker );

  // InteractorStyleImage allows for the following controls:
  // 1) middle mouse + move = camera pan
  // 2) left mouse + move = window/level
  // 3) right mouse + move = camera zoom
  // 4) middle mouse wheel scroll = zoom
  // 5) 'r' = reset window/level
  // 6) shift + 'r' = reset camera
  vtkInteractorStyleImage* imageStyle = imageViewer->GetInteractorStyle();
  imageStyle->AddObserver( vtkCommand::MouseMoveEvent, callback );

  renderWindowInteractor->Initialize();
  renderWindowInteractor->Start();

  return EXIT_SUCCESS;
}